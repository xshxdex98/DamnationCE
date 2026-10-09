/*
P2P_CRYPTO.C

What internet play needs to keep an invite's signalling and its tunnel
private (p2p.c): SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), from
which an invite's topics and keys are derived; ChaCha20-Poly1305 (RFC 8439)
with additional data, which seals the tunnel's packets (a counter for their
nonce) and, with a random nonce sent ahead of the ciphertext, signalling's
messages; X25519 (RFC 7748), with which two machines agree on their
tunnel's keys without sending them; and Ed25519 (RFC 8032), with which a
host signs its public game's listing (p2p_lobby.c); the last three are
Monocypher's (port/third_party/monocypher). The public
brokers carry sealed messages, so only holders of the invite read them; the
tunnel's packets are sealed with keys only its two machines have.
*/

#include "platform.h"
#include "posix.h"
#include "p2p_internal.h"

#include "monocypher.h"
#include "monocypher-ed25519.h"

#include <stdlib.h>
#include <string.h>

/* ---------- SHA-256 */

struct sha256
{
	unsigned int state[8];
	unsigned char block[64];
	unsigned long long length;
	int used;
};

static const unsigned int sha256_constants[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROTATE_RIGHT(value, count) (((value) >> (count)) | ((value) << (32 - (count))))

static void sha256_block(struct sha256 *context, const unsigned char *block)
{
	unsigned int words[64];
	unsigned int a, b, c, d, e, f, g, h;
	int index;

	for (index = 0; index < 16; index++)
	{
		words[index] = (unsigned int)block[index * 4] << 24 | (unsigned int)block[index * 4 + 1] << 16 |
			(unsigned int)block[index * 4 + 2] << 8 | (unsigned int)block[index * 4 + 3];
	}
	for (index = 16; index < 64; index++)
	{
		unsigned int s0 = ROTATE_RIGHT(words[index - 15], 7) ^ ROTATE_RIGHT(words[index - 15], 18) ^
			(words[index - 15] >> 3);
		unsigned int s1 = ROTATE_RIGHT(words[index - 2], 17) ^ ROTATE_RIGHT(words[index - 2], 19) ^
			(words[index - 2] >> 10);

		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}
	a = context->state[0]; b = context->state[1]; c = context->state[2]; d = context->state[3];
	e = context->state[4]; f = context->state[5]; g = context->state[6]; h = context->state[7];
	for (index = 0; index < 64; index++)
	{
		unsigned int s1 = ROTATE_RIGHT(e, 6) ^ ROTATE_RIGHT(e, 11) ^ ROTATE_RIGHT(e, 25);
		unsigned int choice = (e & f) ^ (~e & g);
		unsigned int first = h + s1 + choice + sha256_constants[index] + words[index];
		unsigned int s0 = ROTATE_RIGHT(a, 2) ^ ROTATE_RIGHT(a, 13) ^ ROTATE_RIGHT(a, 22);
		unsigned int majority = (a & b) ^ (a & c) ^ (b & c);
		unsigned int second = s0 + majority;

		h = g; g = f; f = e; e = d + first;
		d = c; c = b; b = a; a = first + second;
	}
	context->state[0] += a; context->state[1] += b; context->state[2] += c; context->state[3] += d;
	context->state[4] += e; context->state[5] += f; context->state[6] += g; context->state[7] += h;
}

static void sha256_begin(struct sha256 *context)
{
	static const unsigned int initial[8] =
	{
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	};

	memcpy(context->state, initial, sizeof(initial));
	context->length = 0;
	context->used = 0;
}

static void sha256_add(struct sha256 *context, const void *data, int size)
{
	const unsigned char *bytes = data;

	context->length += (unsigned long long)size;
	while (size > 0)
	{
		int count = 64 - context->used < size ? 64 - context->used : size;

		memcpy(context->block + context->used, bytes, (size_t)count);
		context->used += count;
		bytes += count;
		size -= count;
		if (context->used == 64)
		{
			sha256_block(context, context->block);
			context->used = 0;
		}
	}
}

static void sha256_end(struct sha256 *context, unsigned char *digest)
{
	unsigned long long bits = context->length * 8;
	unsigned char length[8];
	int index;

	for (index = 0; index < 8; index++)
		length[index] = (unsigned char)(bits >> (56 - index * 8));
	sha256_add(context, "\x80", 1);
	while (context->used != 56)
		sha256_add(context, "", 1);
	sha256_add(context, length, 8);
	for (index = 0; index < 8; index++)
	{
		digest[index * 4] = (unsigned char)(context->state[index] >> 24);
		digest[index * 4 + 1] = (unsigned char)(context->state[index] >> 16);
		digest[index * 4 + 2] = (unsigned char)(context->state[index] >> 8);
		digest[index * 4 + 3] = (unsigned char)context->state[index];
	}
}

void p2p_sha256(const void *data, int size, unsigned char *digest)
{
	struct sha256 context;

	sha256_begin(&context);
	sha256_add(&context, data, size);
	sha256_end(&context, digest);
}

void p2p_hmac_sha256(const unsigned char *key, int key_size, const void *data, int size, unsigned char *digest)
{
	unsigned char block[64], inner[P2P_SHA256_SIZE];
	struct sha256 context;
	int index;

	memset(block, 0, sizeof(block));
	if (key_size > 64)
		p2p_sha256(key, key_size, block);
	else
		memcpy(block, key, (size_t)key_size);
	for (index = 0; index < 64; index++)
		block[index] ^= 0x36;
	sha256_begin(&context);
	sha256_add(&context, block, 64);
	sha256_add(&context, data, size);
	sha256_end(&context, inner);
	for (index = 0; index < 64; index++)
		block[index] ^= 0x36 ^ 0x5c;
	sha256_begin(&context);
	sha256_add(&context, block, 64);
	sha256_add(&context, inner, sizeof(inner));
	sha256_end(&context, digest);
}

/* ---------- ChaCha20-Poly1305 (RFC 8439) */

int p2p_aead_seal(const unsigned char *key, const unsigned char *nonce, const void *additional,
	int additional_size, const void *plaintext, int size, unsigned char *sealed)
{
	/* (a context for the one message: one that wrote more would seal the
	next with a key of its own) */
	crypto_aead_ctx context;

	crypto_aead_init_ietf(&context, key, nonce);
	crypto_aead_write(&context, sealed, sealed + size, additional, (size_t)additional_size, plaintext, (size_t)size);
	crypto_wipe(&context, sizeof(context));
	return size + P2P_TAG_SIZE;
}

int p2p_aead_open(const unsigned char *key, const unsigned char *nonce, const void *additional,
	int additional_size, const unsigned char *sealed, int size, unsigned char *plaintext)
{
	crypto_aead_ctx context;
	int text_size = size - P2P_TAG_SIZE;
	int opened;

	if (text_size < 0)
		return -1;
	crypto_aead_init_ietf(&context, key, nonce);
	opened = crypto_aead_read(&context, plaintext, sealed + text_size, additional, (size_t)additional_size, sealed,
		(size_t)text_size) == 0;
	crypto_wipe(&context, sizeof(context));
	return opened ? text_size : -1;
}

int p2p_seal(const unsigned char *key, const void *plaintext, int size, unsigned char *sealed)
{
	posix_random_bytes(sealed, P2P_NONCE_SIZE);
	return P2P_NONCE_SIZE + p2p_aead_seal(key, sealed, NULL, 0, plaintext, size, sealed + P2P_NONCE_SIZE);
}

int p2p_open(const unsigned char *key, const unsigned char *sealed, int size, unsigned char *plaintext)
{
	if (size < P2P_NONCE_SIZE)
		return -1;
	return p2p_aead_open(key, sealed, NULL, 0, sealed + P2P_NONCE_SIZE, size - P2P_NONCE_SIZE, plaintext);
}

int p2p_equal(const void *first, const void *second, int size)
{
	const unsigned char *a = first, *b = second;
	unsigned char difference = 0;
	int index;

	/* in constant time */
	for (index = 0; index < size; index++)
		difference |= (unsigned char)(a[index] ^ b[index]);
	return difference == 0;
}

/* ---------- X25519 (RFC 7748) */

void p2p_x25519(unsigned char *result, const unsigned char *scalar, const unsigned char *point)
{
	if (point)
		crypto_x25519(result, scalar, point);
	else
		crypto_x25519_public_key(result, scalar);
}

/* ---------- Ed25519 (RFC 8032, with SHA-512), from Monocypher
(port/third_party/monocypher): a run's key, from which its X25519 key also
comes, signs the listing of a public game (p2p_lobby.c) */

void p2p_sha512(const void *data, int size, unsigned char *digest)
{
	crypto_sha512(digest, data, (size_t)size);
}

void p2p_ed25519_public(const unsigned char *seed, unsigned char *public_key, unsigned char *x25519_secret)
{
	unsigned char copy[P2P_SEED_SIZE];
	unsigned char secret_key[64];
	unsigned char hash[64];

	/* (the key pair's making wipes the seed it is given) */
	memcpy(copy, seed, sizeof(copy));
	crypto_ed25519_key_pair(secret_key, public_key, copy);
	if (x25519_secret)
	{
		/* the scalar Ed25519 signs with: X25519 clamps it the same way */
		p2p_sha512(seed, P2P_SEED_SIZE, hash);
		memcpy(x25519_secret, hash, P2P_KEY_SIZE);
	}
	crypto_wipe(secret_key, sizeof(secret_key));
	crypto_wipe(hash, sizeof(hash));
}

void p2p_ed25519_sign(const unsigned char *seed, const unsigned char *public_key, const void *message, int size,
	unsigned char *signature)
{
	unsigned char secret_key[64];

	/* (Monocypher's secret key: the seed, then the public key) */
	memcpy(secret_key, seed, P2P_SEED_SIZE);
	memcpy(secret_key + P2P_SEED_SIZE, public_key, P2P_KEY_SIZE);
	crypto_ed25519_sign(signature, secret_key, message, (size_t)size);
	crypto_wipe(secret_key, sizeof(secret_key));
}

int p2p_ed25519_to_x25519(const unsigned char *public_key, unsigned char *x25519_public)
{
	static const unsigned char zero[P2P_KEY_SIZE];
	static const unsigned char scalar[P2P_KEY_SIZE] = { 1 };
	unsigned char product[P2P_KEY_SIZE];

	crypto_eddsa_to_x25519(x25519_public, public_key);
	/* a point of small order (whose multiple by a clamped scalar, a
	multiple of 8, is 0) has signatures anyone can make, and secrets anyone
	knows */
	crypto_x25519(product, scalar, x25519_public);
	return !p2p_equal(product, zero, P2P_KEY_SIZE);
}

/* ---------- passwords (a password-protected public game's listing holds
its invite's token sealed with the password's key: p2p_lobby.c) */

/* (Argon2id of 4 MiB and three passes: tens of milliseconds, once for the
host and once a guess for a joiner) */
#define PASSWORD_KEY_BLOCKS 4096
#define PASSWORD_KEY_PASSES 3

void p2p_password_key(const char *password, const unsigned char *salt, unsigned char *key)
{
	crypto_argon2_config config = { CRYPTO_ARGON2_ID, PASSWORD_KEY_BLOCKS, PASSWORD_KEY_PASSES, 1 };
	crypto_argon2_inputs inputs;
	void *work_area = malloc((size_t)PASSWORD_KEY_BLOCKS * 1024);

	inputs.pass = (const unsigned char *)password;
	inputs.pass_size = (unsigned int)strlen(password);
	inputs.salt = salt;
	inputs.salt_size = P2P_KEY_SIZE;
	if (!work_area)
	{
		/* (no key anyone could guess: nothing opens with it) */
		posix_random_bytes(key, P2P_PASSWORD_KEY_SIZE);
		return;
	}
	crypto_argon2(key, P2P_PASSWORD_KEY_SIZE, work_area, config, inputs, crypto_argon2_no_extras);
	crypto_wipe(work_area, (size_t)PASSWORD_KEY_BLOCKS * 1024);
	free(work_area);
}

void p2p_seal_token(const unsigned char *key, const unsigned char *signing_key, const unsigned char *token,
	unsigned char *sealed)
{
	/* (a nonce of its own each time: 24 random bytes never repeat) */
	posix_random_bytes(sealed, 24);
	crypto_aead_lock(sealed + 24 + 16, sealed + 24, key, sealed, signing_key, P2P_KEY_SIZE, token, P2P_TOKEN_SIZE);
}

int p2p_unseal_token(const unsigned char *key, const unsigned char *signing_key, const unsigned char *sealed,
	unsigned char *token)
{
	return crypto_aead_unlock(token, sealed + 24, key, sealed, signing_key, P2P_KEY_SIZE, sealed + 24 + 16,
		P2P_TOKEN_SIZE) == 0;
}

int p2p_ed25519_verify(const unsigned char *public_key, const void *message, int size,
	const unsigned char *signature)
{
	unsigned char x25519_public[P2P_KEY_SIZE];

	/* (Monocypher's check turns away an S past the group's order: one
	signature a message) */
	return p2p_ed25519_to_x25519(public_key, x25519_public) &&
		crypto_ed25519_check(signature, public_key, message, (size_t)size) == 0;
}
