/*
CUSTOM_EDITION_SOUNDS.C

The Ogg Vorbis sounds of Halo Custom Edition maps (custom_edition_cache.h).

Halo PC compresses music, dialogue and the announcer as Ogg Vorbis, which
this build's mixer does not play: it plays Xbox ADPCM and uncompressed
samples. When a map is loaded, every Ogg Vorbis permutation is decoded here
(stb_vorbis.c, public domain) and encoded again as Xbox ADPCM, at the
sound's channel count and sample rate, into one buffer kept for as long as
the map is loaded. The permutation is then an Xbox ADPCM one whose samples
lie in a region of the combined offset space of its own
(custom_edition_cache_read serves it from the buffer), so the sound cache
and the mixer see nothing new.

Xbox ADPCM is IMA ADPCM in blocks of 64 samples per channel: a 4-byte
header per channel (the predictor and step index the block starts from),
then 4-byte groups of eight nibbles, low nibble first, alternating between
the channels (port/linux/src/dsound_sdl.c decodes it).
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "sound/sound_definitions.h"
#include "cache_file_formats.h"
#include "custom_edition_cache.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <stdlib.h>
#include <string.h>

/* ---------- constants */

#define SOUND_COMPRESSION_XBOX_ADPCM 1
#define SOUND_COMPRESSION_OGG_VORBIS 3

#define ADPCM_BLOCK_SAMPLES 64
#define ADPCM_BLOCK_BYTES 36
#define ADPCM_STEP_INDEX_MAXIMUM 88

/* the decoded buffer grows by this much at a time */
#define DECODED_GROWTH 0x100000

/* ---------- globals */

static int const adpcm_step_table[ADPCM_STEP_INDEX_MAXIMUM + 1] =
{
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
	253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
	3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
	11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
	32767,
};

static int const adpcm_index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static struct
{
	byte *decoded;
	unsigned long decoded_bytes;
	unsigned long decoded_capacity;
} custom_edition_sounds_globals;

/* ---------- private code */

struct adpcm_state
{
	int predictor;
	int step_index;
};

/* The nibble that brings `state` nearest `sample`, moving it as the decoder will. */
static byte adpcm_encode_sample(
	struct adpcm_state *state,
	int sample)
{
	int step = adpcm_step_table[state->step_index];
	int difference = sample - state->predictor;
	int change = step >> 3;
	byte nibble = 0;

	if (difference < 0)
	{
		nibble = 8;
		difference = -difference;
	}
	if (difference >= step)
	{
		nibble |= 4;
		difference -= step;
		change += step;
	}
	if (difference >= step >> 1)
	{
		nibble |= 2;
		difference -= step >> 1;
		change += step >> 1;
	}
	if (difference >= step >> 2)
	{
		nibble |= 1;
		change += step >> 2;
	}

	state->predictor += (nibble & 8) ? -change : change;
	state->predictor = PIN(state->predictor, -32768, 32767);
	state->step_index += adpcm_index_table[nibble & 7];
	state->step_index = PIN(state->step_index, 0, ADPCM_STEP_INDEX_MAXIMUM);

	return nibble;
}

/* Encodes `frame_count` frames of interleaved 16-bit samples as Xbox ADPCM into
`output`, which holds adpcm_encoded_bytes(frame_count, channel_count) bytes.
A last block short of 64 frames is filled with the final sample. */
static unsigned long adpcm_encoded_bytes(
	long frame_count,
	long channel_count)
{
	return (unsigned long)((frame_count + ADPCM_BLOCK_SAMPLES - 1) / ADPCM_BLOCK_SAMPLES) *
		ADPCM_BLOCK_BYTES * channel_count;
}

static void adpcm_encode(
	short const *samples,
	long frame_count,
	long channel_count,
	byte *output)
{
	struct adpcm_state states[2] = { { 0, 0 }, { 0, 0 } };
	long block_count = (frame_count + ADPCM_BLOCK_SAMPLES - 1) / ADPCM_BLOCK_SAMPLES;
	long block_index;

	for (block_index = 0; block_index < block_count; block_index++)
	{
		byte *block = output + block_index * ADPCM_BLOCK_BYTES * channel_count;
		byte *nibbles = block + 4 * channel_count;
		long channel;

		for (channel = 0; channel < channel_count; channel++)
		{
			struct adpcm_state *state = &states[channel];
			byte *header = block + channel * 4;
			long group;

			header[0] = (byte)(state->predictor & 0xFF);
			header[1] = (byte)((state->predictor >> 8) & 0xFF);
			header[2] = (byte)state->step_index;
			header[3] = 0;
			for (group = 0; group < 8; group++)
			{
				byte *group_bytes = nibbles + (group * channel_count + channel) * 4;
				long byte_index;

				for (byte_index = 0; byte_index < 4; byte_index++)
				{
					long frame = block_index * ADPCM_BLOCK_SAMPLES + group * 8 + byte_index * 2;
					long first = MIN(frame, frame_count - 1);
					long second = MIN(frame + 1, frame_count - 1);
					byte low = adpcm_encode_sample(state, samples[first * channel_count + channel]);
					byte high = adpcm_encode_sample(state, samples[second * channel_count + channel]);

					group_bytes[byte_index] = (byte)(low | (high << 4));
				}
			}
		}
	}

	return;
}

/* Interleaved 16-bit frames of the Ogg Vorbis stream `data` at the sound's
channel count and sample rate (a mono stream fills both channels, a stereo
one is averaged for a mono sound, and another rate is resampled by the
nearest frame); NULL when the stream cannot be decoded. The caller frees
the frames. */
static short *vorbis_decode(
	byte const *data,
	long data_bytes,
	long channel_count,
	long sample_rate,
	long *frame_count)
{
	int stream_channels;
	int stream_rate;
	short *stream_samples;
	int stream_frames;
	short *samples;
	long frame;

	stream_frames = stb_vorbis_decode_memory(data, data_bytes, &stream_channels, &stream_rate, &stream_samples);
	if (stream_frames <= 0 || stream_channels <= 0 || stream_rate <= 0)
	{
		return NULL;
	}

	*frame_count = (long)((double)stream_frames * sample_rate / stream_rate);
	samples = malloc((size_t)*frame_count * channel_count * sizeof(*samples));
	if (samples)
	{
		for (frame = 0; frame < *frame_count; frame++)
		{
			long source = MIN((long)((double)frame * stream_rate / sample_rate), (long)stream_frames - 1);
			short const *in = stream_samples + source * stream_channels;
			short *out = samples + frame * channel_count;

			if (channel_count == stream_channels)
			{
				memcpy(out, in, channel_count * sizeof(*out));
			}
			else if (channel_count == 2)
			{
				out[0] = out[1] = in[0];
			}
			else
			{
				out[0] = (short)((in[0] + in[1]) / 2);
			}
		}
	}
	/* (stb_vorbis.c is an object of its own, which allocates with the C
	library's malloc, not cseries.h's debug_malloc: the parentheses keep
	cseries.h's free macro, debug_free, from taking it) */
	(free)(stream_samples);

	return samples;
}

/* Room for `bytes` more in the decoded buffer: its offset, or NONE. */
static long decoded_reserve(
	unsigned long bytes)
{
	unsigned long needed = custom_edition_sounds_globals.decoded_bytes + bytes;

	if (needed > custom_edition_sounds_globals.decoded_capacity)
	{
		unsigned long capacity = (needed + DECODED_GROWTH - 1) / DECODED_GROWTH * DECODED_GROWTH;
		byte *grown = realloc(custom_edition_sounds_globals.decoded, capacity);

		if (!grown)
		{
			return NONE;
		}
		custom_edition_sounds_globals.decoded = grown;
		custom_edition_sounds_globals.decoded_capacity = capacity;
	}

	return (long)custom_edition_sounds_globals.decoded_bytes;
}

/* Makes the Ogg Vorbis permutation `permutation` of `sound` an Xbox ADPCM
one whose samples are in the decoded buffer; FALSE when it cannot be. */
static boolean permutation_decode(
	struct sound_definition const *sound,
	struct sound_permutation *permutation,
	long decoded_offset)
{
	long channel_count = sound->encoding == 0 ? 1 : 2;
	long sample_rate = sound->sample_rate == 0 ? 22050 : 44100;
	byte *data;
	short *samples;
	long frame_count;
	unsigned long encoded_bytes;
	long offset;

	data = malloc(permutation->samples.size);
	if (!data)
	{
		return FALSE;
	}
	custom_edition_cache_read(NONE, permutation->samples.file_offset, permutation->samples.size, data);
	samples = vorbis_decode(data, permutation->samples.size, channel_count, sample_rate, &frame_count);
	free(data);
	if (!samples)
	{
		return FALSE;
	}

	encoded_bytes = adpcm_encoded_bytes(frame_count, channel_count);
	offset = decoded_reserve(encoded_bytes);
	if (offset != NONE)
	{
		adpcm_encode(samples, frame_count, channel_count, custom_edition_sounds_globals.decoded + offset);
		custom_edition_sounds_globals.decoded_bytes += encoded_bytes;
		permutation->compression = SOUND_COMPRESSION_XBOX_ADPCM;
		permutation->samples.file_offset = decoded_offset + offset;
		permutation->samples.size = (long)encoded_bytes;
		permutation->sample_buffer_size = 0;
	}
	free(samples);

	return offset != NONE;
}

/* ---------- public code */

boolean custom_edition_sounds_decode(
	byte *tag_cache,
	unsigned long loaded_bytes,
	long decoded_offset)
{
	struct sound_definition *sound;
	int32_t tag_index = NONE;
	long decoded_count = 0;
	long failed_count = 0;

	while ((sound = custom_edition_cache_tag_next(tag_cache, loaded_bytes, SOUND_DEFINITION_TAG, sizeof(*sound), &tag_index)) != NULL)
	{
		boolean decoded = TRUE;
		long range_index;

		for (range_index = 0; range_index < sound->pitch_ranges.count; range_index++)
		{
			struct sound_pitch_range *range = custom_edition_cache_block_element(
				tag_cache, loaded_bytes, &sound->pitch_ranges, range_index, sizeof(*range));
			long permutation_index;

			for (permutation_index = 0; range && permutation_index < range->permutations.count; permutation_index++)
			{
				struct sound_permutation *permutation = custom_edition_cache_block_element(
					tag_cache, loaded_bytes, &range->permutations, permutation_index, sizeof(*permutation));

				if (permutation && permutation->compression == SOUND_COMPRESSION_OGG_VORBIS)
				{
					if (permutation_decode(sound, permutation, decoded_offset))
					{
						decoded_count++;
					}
					else
					{
						decoded = FALSE;
					}
				}
			}
		}
		if (sound->compression == SOUND_COMPRESSION_OGG_VORBIS)
		{
			sound->compression = SOUND_COMPRESSION_XBOX_ADPCM;
		}
		if (!decoded)
		{
			/* with no pitch ranges the game neither plays nor loads the sound */
			error(_error_silent, "custom edition: cannot decode the Ogg Vorbis sound '%s'; it will not play",
				custom_edition_cache_tag_name(tag_cache, loaded_bytes, tag_index));
			sound->pitch_ranges.count = 0;
			failed_count++;
		}
	}
	if (decoded_count || failed_count)
	{
		error(_error_silent, "custom edition: %ld Ogg Vorbis sound permutations decoded to %lu bytes of Xbox ADPCM%s",
			decoded_count, custom_edition_sounds_globals.decoded_bytes,
			failed_count ? " (some sounds could not be decoded)" : "");
	}

	return TRUE;
}

boolean custom_edition_sounds_read(
	long offset,
	long size,
	void *buffer)
{
	if (offset < 0 || size < 0 ||
		(unsigned long)offset + (unsigned long)size > custom_edition_sounds_globals.decoded_bytes)
	{
		return FALSE;
	}
	memcpy(buffer, custom_edition_sounds_globals.decoded + offset, size);

	return TRUE;
}

void custom_edition_sounds_dispose(
	void)
{
	free(custom_edition_sounds_globals.decoded);
	memset(&custom_edition_sounds_globals, 0, sizeof(custom_edition_sounds_globals));

	return;
}
