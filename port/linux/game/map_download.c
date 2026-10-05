/*
MAP_DOWNLOAD.C

A client joining a game on a custom map it doesn't have gets the map from
the host, once the player says so.

The client asks the host how big the map and its picture (<name>.bmp, its
thumbnail in the menus) are, shows the player, and on DOWNLOAD asks for
them a piece at a time over the connection's reliable stream, at most
WINDOW_BYTES ahead of what has arrived. Each file is written as a .part file
in DOWNLOADED_MAPS_DIRECTORY and kept only if its contents check out: a map
must be a Halo cache (a Custom Edition one, or an Xbox one of a build this
game plays), and a picture a bitmap the menus can read. The map is then
loaded, and the game joined, as if it had been there.

Nothing else can be sent. The host sends only the map its game is on and
the picture beside it, never a stock map, and only by a plain file name; the
client keeps only those two files, under its own names for them, in its
own folder.
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "cache/cache_files.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager_internal.h"
#include "interface/event_manager.h"
#include "input/input.h"

#include "bmp_files.h"
#include "custom_edition_cache.h"
#include "custom_edition_maps.h"
#include "map_download.h"
#include "network_distributed.h"

#include "interface/ui_widget_instance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	_map_download_map,
	_map_download_picture,
	NUMBER_OF_MAP_DOWNLOAD_FILES
};

/* a file name, without its extension (custom_edition_maps.c takes names of
up to 56 characters) */
#define MAP_NAME_SIZE 64
#define PATH_SIZE 256

/* the most of a file one answer carries, one request asks for, and the
client has asked for beyond what has arrived (well inside the reliable
stream's 256 KB queue) */
#define CHUNK_BYTES 0xE00
#define REQUEST_BYTES 0x8000
#define WINDOW_BYTES 0x10000

/* the largest map (a Custom Edition one may be over the Xbox's 0x11600000)
and picture taken */
#define MAXIMUM_MAP_BYTES 0x18000000L
#define MAXIMUM_PICTURE_BYTES 0x400000L

/* a cache file's header, both formats: 'head' at its start, 'foot' at its
end (cache_files.c's CACHE_FILE_HEADER_SIGNATURE, little-endian) */
#define CACHE_HEADER_BYTES 0x800
#define CACHE_FOOTER_OFFSET 0x7FC

/* how long the client waits for the host to answer before giving up (and
the host, before taking a client asking nothing for idle) */
#define SILENCE_MILLISECONDS 15000
/* while the player decides, how often the client asks the host again, so the
host doesn't take it for idle */
#define KEEP_ALIVE_MILLISECONDS 5000

/* maps everyone has, which are never sent: the Xbox's campaign and
multiplayer levels, Halo PC's own multiplayer maps, and the resource maps */
static char const *const stock_map_names[] =
{
	"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout",
	"carousel", "boardingaction", "bloodgulch", "wizard", "putput", "longest",
	"icefields", "deathisland", "dangercanyon", "infinity", "timberland", "gephyrophobia",
	"ui", "bitmaps", "sounds", "loc",
};

/* ---------- structures */

/* client to host: a file's size (offset NONE), or a piece of it */
struct map_download_request
{
	struct distributed_message_header header;
	char map_name[MAP_NAME_SIZE];
	byte file;
	byte pad[3];
	long offset;
	long length;
};

/* host to client: a file's size (NONE: the host won't send it), and with an
offset, a piece of it */
struct map_download_answer
{
	struct distributed_message_header header;
	char map_name[MAP_NAME_SIZE];
	byte file;
	byte pad;
	word data_size;
	long size;
	long offset;
	byte data[CHUNK_BYTES];
};

#define ANSWER_HEAD_BYTES (sizeof(struct map_download_answer) - CHUNK_BYTES)

/* (both fit one network message) */
typedef char map_download_answer_size_assert[sizeof(struct map_download_answer) <= 0xFFF ? 1 : -1];

/* ---------- prototypes */

/* network_game_globals.c's and network_server_message_handler.c's */
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);

/* ---------- private code */

/* a plain file name: letters, digits and _ - . and spaces, not starting with
a dot, no ".." and no path, and none of the stock maps */
static boolean map_name_allowed(
	char const *name)
{
	size_t length = strlen(name);
	size_t index;

	if (length == 0 || length >= MAP_NAME_SIZE || name[0] == '.' || strstr(name, ".."))
		return FALSE;
	for (index = 0; index < length; index++)
	{
		char character = name[index];

		if (!(character >= 'a' && character <= 'z') && !(character >= 'A' && character <= 'Z') &&
			!(character >= '0' && character <= '9') && !strchr("_-. ", character))
		{
			return FALSE;
		}
	}
	for (index = 0; index < NUMBEROF(stock_map_names); index++)
	{
		if (!csstrcasecmp(name, stock_map_names[index]))
			return FALSE;
	}

	return TRUE;
}

static long maximum_file_bytes(
	byte file)
{
	return file == _map_download_map ? MAXIMUM_MAP_BYTES : MAXIMUM_PICTURE_BYTES;
}

/* ---------- the host */

/* when each client machine last asked for some of the map */
static unsigned long host_request_times[HALO_PORT_MAXIMUM_NETWORK_MACHINES];

/* the file a request names: the game's own map, or the picture beside it */
static boolean host_file_path(
	char const *map_name,
	byte file,
	char path[PATH_SIZE])
{
	struct network_game_server *server = global_network_game_server_get();
	size_t length;

	if (!server || file >= NUMBER_OF_MAP_DOWNLOAD_FILES || !map_name_allowed(map_name) ||
		csstrcasecmp(map_name, tag_name_strip_path(network_game_server_get_game(server)->map.name)))
	{
		return FALSE;
	}
	if (!custom_edition_cache_map_file(map_name, path) && !cache_files_map_path(map_name, path))
		return FALSE;
	length = strlen(path);
	if (length < 4 || csstrcasecmp(path + length - 4, ".map"))
		return FALSE;
	if (file == _map_download_picture)
		strcpy(path + length - 4, ".bmp");

	return TRUE;
}

static void host_send_answer(
	long machine_index,
	struct map_download_answer *answer)
{
	word size = (word)(ANSWER_HEAD_BYTES + answer->data_size);

	distributed_fill_header(answer, _distributed_message_map_answer, 1, size);
	network_distributed_server_send_to_machine_reliably(machine_index, answer, size);
}

/* answers a client's request: the file's size, or the piece it asked for */
static void host_handle_request(
	long machine_index,
	struct map_download_request const *request)
{
	struct map_download_answer answer;
	char path[PATH_SIZE];
	FILE *stream = NULL;
	long size = NONE;

	if (machine_index >= 0 && machine_index < HALO_PORT_MAXIMUM_NETWORK_MACHINES)
		host_request_times[machine_index] = system_milliseconds();
	csmemset(&answer, 0, ANSWER_HEAD_BYTES);
	csmemcpy(answer.map_name, request->map_name, MAP_NAME_SIZE);
	answer.map_name[MAP_NAME_SIZE - 1] = 0;
	answer.file = request->file;
	answer.offset = NONE;
	if (host_file_path(answer.map_name, answer.file, path) && (stream = fopen(path, "rb")) != NULL &&
		fseek(stream, 0, SEEK_END) == 0)
	{
		size = ftell(stream);
		if (size <= 0 || size > maximum_file_bytes(answer.file))
			size = NONE;
	}
	answer.size = size;
	if (request->offset == NONE || size == NONE)
	{
		host_send_answer(machine_index, &answer);
	}
	/* (a piece within the file and the most asked for at once: else the
	request is a bad client's, and goes unanswered) */
	else if (request->offset >= 0 && request->length > 0 && request->length <= REQUEST_BYTES &&
		request->offset <= size - request->length && fseek(stream, request->offset, SEEK_SET) == 0)
	{
		long sent;

		for (sent = 0; sent < request->length; sent += answer.data_size)
		{
			answer.offset = request->offset + sent;
			answer.data_size = (word)MIN(CHUNK_BYTES, request->length - sent);
			if (fread(answer.data, 1, answer.data_size, stream) != answer.data_size)
				break;
			host_send_answer(machine_index, &answer);
		}
	}
	if (stream)
		fclose(stream);
}

/* ---------- the client */

#ifdef HALO_GAME_BROWSER

enum
{
	_client_idle,
	/* asking the player, before joining, whether to download the game's map */
	_client_asking_join,
	/* asked the host for the files' sizes */
	_client_sizing,
	/* asking the player */
	_client_asking,
	_client_downloading,
	/* stopped, telling the player why */
	_client_failed,
};

static struct
{
	short state;
	/* the host's map, as its game names it (levels\...\<name>), and its file name */
	char map_name[0x80];
	char name[MAP_NAME_SIZE];
	/* each file's size from the host (NONE: none to send), and which are known */
	long sizes[NUMBER_OF_MAP_DOWNLOAD_FILES];
	short sizes_known;
	/* the file being downloaded, how much of it has arrived and been asked for */
	byte file;
	long received;
	long requested;
	FILE *stream;
	unsigned long heard_time;
	char failure[128];
	/* the host began the game in progress before the map was here
(map_download_hold_begin) */
	boolean begin_held;
	/* the map the player said to download on JOIN (map_download_ask), and
whether this download is it, so isn't asked about again */
	char approved_name[MAP_NAME_SIZE];
	boolean approved;
	void (*join)(void);
	/* when the client last asked the host anything */
	unsigned long asked_time;
} client;

static void client_path(
	byte file,
	boolean part,
	char path[PATH_SIZE])
{
	snprintf(path, PATH_SIZE, "%s%s%s%s", DOWNLOADED_MAPS_DIRECTORY, client.name,
		file == _map_download_map ? ".map" : ".bmp", part ? ".part" : "");
}

static void client_send_request(
	byte file,
	long offset,
	long length)
{
	struct map_download_request request;

	csmemset(&request, 0, sizeof(request));
	csmemcpy(request.map_name, client.name, MAP_NAME_SIZE);
	request.file = file;
	request.offset = offset;
	request.length = length;
	distributed_fill_header(&request, _distributed_message_map_request, 1, sizeof(request));
	network_distributed_client_send_reliably(&request, sizeof(request));
	client.asked_time = system_milliseconds();
}

/* closes and deletes a file half downloaded */
static void client_discard_part(
	void)
{
	char path[PATH_SIZE];

	if (!client.stream)
		return;
	fclose(client.stream);
	client.stream = NULL;
	client_path(client.file, TRUE, path);
	DeleteFileA(path);
}

static void client_forget(
	void)
{
	client_discard_part();
	client.state = _client_idle;
}

static void client_fail(
	char const *reason)
{
	client_discard_part();
	snprintf(client.failure, sizeof(client.failure), "%s", reason);
	client.state = _client_failed;
	error(_error_silent, "map download: %s: %s", client.name, reason);
}

/* the player chose to go: the download dropped, and the game left */
static void client_leave(
	void)
{
	client_forget();
	network_game_abort();
}

/* asks for more of the file, up to WINDOW_BYTES beyond what has arrived */
static void client_request_more(
	void)
{
	long size = client.sizes[client.file];

	while (client.requested < size && client.requested - client.received < WINDOW_BYTES)
	{
		long length = MIN(REQUEST_BYTES, size - client.requested);

		client_send_request(client.file, client.requested, length);
		client.requested += length;
	}
}

static boolean client_start_file(
	byte file)
{
	char path[PATH_SIZE];

	CreateDirectoryA(DOWNLOADED_MAPS_DIRECTORY, NULL);
	client.file = file;
	client.received = 0;
	client.requested = 0;
	client_path(file, TRUE, path);
	client.stream = fopen(path, "wb");
	if (!client.stream)
		return FALSE;
	client.heard_time = system_milliseconds();
	client_request_more();

	return TRUE;
}

/* whether the picture downloaded is a bitmap the menus can read */
static boolean client_picture_valid(
	char const *path)
{
	FILE *stream = fopen(path, "rb");
	struct bmp_file_picture picture;
	uint8_t *contents = NULL;
	long size = 0;
	boolean valid = FALSE;

	if (!stream)
		return FALSE;
	if (fseek(stream, 0, SEEK_END) == 0 && (size = ftell(stream)) > 0 && size <= MAXIMUM_PICTURE_BYTES &&
		fseek(stream, 0, SEEK_SET) == 0 && (contents = malloc((size_t)size)) != NULL &&
		fread(contents, 1, (size_t)size, stream) == (size_t)size)
	{
		valid = bmp_file_open(contents, (uint32_t)size, &picture) == _bmp_file_status_ok;
	}
	free(contents);
	fclose(stream);

	return valid;
}

static void client_start_download(
	void)
{
	client.state = _client_downloading;
	if (!client_start_file(_map_download_map))
		client_fail("The map couldn't be\nsaved. Is the disk\nfull?");
}

/* the map is here: loaded, and with it the game joined (the game the host
began meanwhile, now) */
static void client_done(
	void)
{
	struct network_game_client *network_client = global_network_game_client_get();

	error(_error_silent, "map download: %s: done", client.name);
	client.state = _client_idle;
	custom_edition_maps_look_again();
	main_set_multiplayer_map_name(client.map_name);
	if (client.begin_held && network_client && !network_game_client_game_has_started(network_client))
		error(_error_silent, "map download: %s: the game the host began couldn't be joined", client.name);
	client.begin_held = FALSE;
}

/* the file being downloaded can't be had: the map is the point, and the
download stops; without the picture, the menus show the unknown level's */
static void client_file_failed(
	char const *reason)
{
	if (client.file == _map_download_map)
	{
		client_fail(reason);
		return;
	}
	client_discard_part();
	error(_error_silent, "map download: %s: no picture: %s", client.name, reason);
	client_done();
}

/* whether a file's first piece starts as its kind does: a cache header, or
a bitmap's "BM" (nothing more of a file is taken that doesn't) */
static boolean client_first_piece_valid(
	byte file,
	byte const *data,
	word size)
{
	if (file == _map_download_map)
	{
		return size >= CACHE_HEADER_BYTES && !memcmp(data, "daeh", 4) &&
			!memcmp(data + CACHE_FOOTER_OFFSET, "toof", 4);
	}

	return size >= 2 && !memcmp(data, "BM", 2);
}

/* whether a whole file is what it should be, by its contents */
static boolean client_file_valid(
	byte file,
	char const *path)
{
	if (file == _map_download_map)
		return custom_edition_cache_file_is_map(path) || cache_files_xbox_map_playable(path);

	return client_picture_valid(path);
}

/* a file has all arrived: kept under its own name if it checks out, else
deleted; then the picture, if the host has one */
static void client_finish_file(
	void)
{
	char part_path[PATH_SIZE];
	char path[PATH_SIZE];

	fclose(client.stream);
	client.stream = NULL;
	client_path(client.file, TRUE, part_path);
	client_path(client.file, FALSE, path);
	if (!client_file_valid(client.file, part_path))
	{
		DeleteFileA(part_path);
		client_file_failed(client.file == _map_download_map ?
			"What the host sent\nisn't a Halo map this\ngame can play. It\nwas deleted." :
			"it isn't a bitmap");
		return;
	}
	DeleteFileA(path);
	MoveFileA(part_path, path);
	if (client.file == _map_download_map && client.sizes[_map_download_picture] != NONE)
	{
		if (!client_start_file(_map_download_picture))
			client_file_failed("it couldn't be saved");
		return;
	}
	client_done();
}

/* the host's answer to the size requests: once both are in, the player is
asked */
static void client_handle_size(
	struct map_download_answer const *answer)
{
	long map_size;

	client.sizes[answer->file] = answer->size > 0 && answer->size <= maximum_file_bytes(answer->file) ?
		answer->size : NONE;
	SET_FLAG(client.sizes_known, answer->file, TRUE);
	if (!TEST_FLAG(client.sizes_known, _map_download_map))
		return;
	map_size = client.sizes[_map_download_map];
	if (map_size == NONE)
		client_fail("The host can't send\nthis map.");
	else if (map_size < CACHE_HEADER_BYTES)
		client_fail("The host's map isn't\na Halo map.");
	else if (TEST_FLAG(client.sizes_known, _map_download_picture))
	{
		if (client.approved)
			client_start_download();
		else
			client.state = _client_asking;
	}
}

/* a piece of the file being downloaded, the next one expected */
static void client_handle_piece(
	struct map_download_answer const *answer)
{
	if (answer->file != client.file || answer->offset != client.received ||
		answer->size != client.sizes[client.file] || client.received + answer->data_size > client.sizes[client.file])
	{
		return;
	}
	if (answer->offset == 0 && !client_first_piece_valid(client.file, answer->data, answer->data_size))
	{
		client_file_failed(client.file == _map_download_map ?
			"What the host is\nsending isn't a Halo\nmap. Nothing was kept." :
			"it isn't a bitmap");
		return;
	}
	if (fwrite(answer->data, 1, answer->data_size, client.stream) != answer->data_size)
	{
		client_file_failed(client.file == _map_download_map ?
			"The map couldn't be\nsaved. Is the disk\nfull?" :
			"it couldn't be saved");
		return;
	}
	client.received += answer->data_size;
	if (client.received == client.sizes[client.file])
		client_finish_file();
	else
		client_request_more();
}

static void client_handle_answer(
	struct map_download_answer const *answer,
	word size)
{
	char name[MAP_NAME_SIZE];

	csmemcpy(name, answer->map_name, MAP_NAME_SIZE);
	name[MAP_NAME_SIZE - 1] = 0;
	/* (only answers about the map being downloaded, whole) */
	if (csstrcmp(name, client.name) || answer->file >= NUMBER_OF_MAP_DOWNLOAD_FILES ||
		answer->data_size > CHUNK_BYTES || size != ANSWER_HEAD_BYTES + answer->data_size)
	{
		return;
	}
	if (client.state == _client_sizing && answer->offset == NONE)
	{
		client.heard_time = system_milliseconds();
		client_handle_size(answer);
	}
	else if (client.state == _client_downloading && answer->offset != NONE)
	{
		client.heard_time = system_milliseconds();
		client_handle_piece(answer);
	}
}

/* (the client's machine gone from the game: whatever it was downloading
dropped; the question before joining needs no game) */
static void client_forget_if_disconnected(
	void)
{
	if (client.state != _client_idle && client.state != _client_asking_join && !global_network_game_client_get())
		client_forget();
}

#endif

/* ---------- public code */

boolean map_download_message(
	byte type)
{
	return type == _distributed_message_map_request || type == _distributed_message_map_answer;
}

void map_download_handle_message(
	long machine_index,
	byte type,
	void const *data,
	word size)
{
	if (type == _distributed_message_map_request)
	{
		if (machine_index != NONE && size == sizeof(struct map_download_request))
			host_handle_request(machine_index, (struct map_download_request const *)data);
		return;
	}
#ifdef HALO_GAME_BROWSER
	if (machine_index == NONE && size >= ANSWER_HEAD_BYTES && size <= sizeof(struct map_download_answer))
		client_handle_answer((struct map_download_answer const *)data, size);
#endif
}

boolean map_download_machine_busy(
	long machine_index)
{
	return machine_index >= 0 && machine_index < HALO_PORT_MAXIMUM_NETWORK_MACHINES &&
		host_request_times[machine_index] &&
		system_milliseconds() - host_request_times[machine_index] < SILENCE_MILLISECONDS;
}

boolean map_download_needed(
	char const *map_name)
{
	char const *name = tag_name_strip_path(map_name);
	char path[PATH_SIZE];

	return map_name_allowed(name) && !custom_edition_cache_map_file(name, path) && !cache_files_map_path(name, path);
}

#ifdef HALO_GAME_BROWSER

/* ---------- the dialog */

/* the dialog (tools/port_settings.py's _map_download: ce/map_download.xml),
by the name the menus load it by, and the last part they know it by */
#define DIALOG_WIDGET "pc\\map_download\\map_download_screen"
#define DIALOG_WIDGET_NAME "map_download_screen"
/* the most of a map's name one of its lines shows */
#define DIALOG_NAME_CHARACTERS 22

/* source/interface/ui_widget.c's */
boolean ui_widget_port_open_from_top(char const *name);
void ui_widget_port_go_back_from_top(void);
struct widget_instance *ui_widget_port_top(void);

static boolean dialog_up(
	void)
{
	struct widget_instance *top = ui_widget_port_top();

	return top && top->name && !strcmp(top->name, DIALOG_WIDGET_NAME);
}

static void megabytes(
	long bytes,
	char *text,
	size_t size)
{
	snprintf(text, size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
}

/* the map's name as the dialog shows it, cut to a line */
static void dialog_map_name(
	char *text,
	size_t size)
{
	if (strlen(client.name) <= DIALOG_NAME_CHARACTERS)
		snprintf(text, size, "%s", client.name);
	else
		snprintf(text, size, "%.*s...", DIALOG_NAME_CHARACTERS - 3, client.name);
}

boolean map_download_begin(
	char const *map_name)
{
	char approved_name[MAP_NAME_SIZE];

	if (!global_network_game_client_get())
		return FALSE;
	client_forget();
	csmemcpy(approved_name, client.approved_name, sizeof(approved_name));
	csmemset(&client, 0, sizeof(client));
	snprintf(client.map_name, sizeof(client.map_name), "%s", map_name);
	snprintf(client.name, sizeof(client.name), "%s", tag_name_strip_path(map_name));
	client.approved = !csstrcasecmp(approved_name, client.name);
	client.sizes[_map_download_map] = NONE;
	client.sizes[_map_download_picture] = NONE;
	client.state = _client_sizing;
	client.heard_time = system_milliseconds();
	client_send_request(_map_download_map, NONE, 0);
	client_send_request(_map_download_picture, NONE, 0);

	return TRUE;
}

boolean map_download_ask(
	char const *map_name,
	void (*join)(void))
{
	if (!map_name_allowed(tag_name_strip_path(map_name)))
		return FALSE;
	client_forget();
	csmemset(&client, 0, sizeof(client));
	snprintf(client.map_name, sizeof(client.map_name), "%s", map_name);
	snprintf(client.name, sizeof(client.name), "%s", tag_name_strip_path(map_name));
	client.join = join;
	client.state = _client_asking_join;

	return TRUE;
}

boolean map_download_holds_players(
	void)
{
	return client.state != _client_idle && client.state != _client_asking_join;
}

boolean map_download_hold_begin(
	void)
{
	if (client.state == _client_idle)
		return FALSE;
	client.begin_held = TRUE;

	return TRUE;
}

void map_download_dialog(
	char *text,
	size_t text_size,
	char const **accept,
	char const **cancel)
{
	unsigned long now = system_milliseconds();
	char name[DIALOG_NAME_CHARACTERS + 4];
	char sizes[NUMBER_OF_MAP_DOWNLOAD_FILES][24];
	char received[24];

	/* (the download's clock: a host gone quiet ends it, and while the player
	decides, the host hears this machine isn't idle) */
	if ((client.state == _client_sizing || client.state == _client_downloading) &&
		now - client.heard_time > SILENCE_MILLISECONDS)
	{
		client_fail("The host stopped\nanswering. It may be\non a version that\ncan't send maps.");
	}
	if (client.state == _client_asking && now - client.asked_time > KEEP_ALIVE_MILLISECONDS)
		client_send_request(_map_download_map, NONE, 0);

	dialog_map_name(name, sizeof(name));
	megabytes(client.sizes[_map_download_map], sizes[_map_download_map], sizeof(sizes[0]));
	megabytes(client.sizes[_map_download_picture], sizes[_map_download_picture], sizeof(sizes[0]));
	megabytes(client.received, received, sizeof(received));
	*accept = NULL;
	*cancel = "CANCEL";
	switch (client.state)
	{
	case _client_asking_join:
		snprintf(text, text_size, "%s\nisn't installed.\nDownload it from the\nhost and join?", name);
		*accept = "DOWNLOAD";
		break;
	case _client_sizing:
		snprintf(text, text_size, "Asking the host for\n%s...", name);
		break;
	case _client_asking:
		snprintf(text, text_size, "%s\nisn't installed.\nDownload it from the\nhost? (%s)", name,
			sizes[_map_download_map]);
		*accept = "DOWNLOAD";
		*cancel = "LEAVE";
		break;
	case _client_downloading:
		if (client.file == _map_download_map)
			snprintf(text, text_size, "Downloading\n%s\n%s of %s", name, received, sizes[_map_download_map]);
		else
			snprintf(text, text_size, "Downloading its\npicture\n%s of %s", received, sizes[_map_download_picture]);
		break;
	case _client_failed:
		snprintf(text, text_size, "%s", client.failure);
		*cancel = "OK";
		break;
	default:
		text[0] = 0;
		break;
	}
}

void map_download_dialog_press(
	boolean accept)
{
	if (client.state == _client_asking_join)
	{
		/* (yes: the game joined, and the map fetched once the host is
		reached; no: nothing joined) */
		client.state = _client_idle;
		if (accept)
		{
			snprintf(client.approved_name, sizeof(client.approved_name), "%s", client.name);
			client.join();
		}
	}
	else if (client.state == _client_asking && accept)
	{
		client_start_download();
	}
	else
	{
		client_leave();
	}
}

void map_download_menus_update(
	void)
{
	client_forget_if_disconnected();
	if (client.state != _client_idle && !dialog_up())
		ui_widget_port_open_from_top(DIALOG_WIDGET);
	else if (client.state == _client_idle && dialog_up())
		ui_widget_port_go_back_from_top();
}

boolean map_download_dialog_up(
	void)
{
	return dialog_up();
}

#else

boolean map_download_begin(
	char const *map_name)
{
	return FALSE;
}

boolean map_download_ask(
	char const *map_name,
	void (*join)(void))
{
	return FALSE;
}

boolean map_download_holds_players(
	void)
{
	return FALSE;
}

boolean map_download_hold_begin(
	void)
{
	return FALSE;
}

void map_download_dialog(
	char *text,
	size_t text_size,
	char const **accept,
	char const **cancel)
{
	text[0] = 0;
	*accept = NULL;
	*cancel = "OK";
}

void map_download_dialog_press(
	boolean accept)
{
}

void map_download_menus_update(
	void)
{
}

boolean map_download_dialog_up(
	void)
{
	return FALSE;
}

#endif
