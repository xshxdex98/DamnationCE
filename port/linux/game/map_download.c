/*
MAP_DOWNLOAD.C

Downloads a custom map from the host when a player joins a game on a map
they don't have, after asking them.

The client asks the host for the sizes of the map and its picture
(<name>.bmp, the thumbnail the menus show), then asks the player. If they
accept, it requests the files in pieces over the connection's reliable
stream, keeping at most WINDOW_BYTES requested beyond what has arrived.
Each file is written to a .part file in DOWNLOADED_MAPS_DIRECTORY and only
kept if its contents check out: the map has to be a Halo cache file this
game can play (Custom Edition or Xbox), and the picture a bitmap the menus
can read. After that the map loads and the game is joined as if the map
had been installed all along.

Nothing else can be sent. The host only sends the map its game is running
and the picture next to it, never a stock map, and only by a plain file
name. The client only writes those two files, named by itself, in its own
folder.
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

#ifdef HALO_GAME_BROWSER
#include "overlay_screens.h"
#include "../src/ui_overlay.h"
#endif

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

/* a map's file name without the extension (custom_edition_maps.c allows
up to 56 characters) */
#define MAP_NAME_SIZE 64
#define PATH_SIZE 256

/* bytes per answer, per request, and the most the client keeps requested
beyond what has arrived (well inside the reliable stream's 256 KB queue) */
#define CHUNK_BYTES 0xE00
#define REQUEST_BYTES 0x8000
#define WINDOW_BYTES 0x10000

/* size limits; Custom Edition maps can be bigger than the Xbox's 0x11600000 */
#define MAXIMUM_MAP_BYTES 0x18000000L
#define MAXIMUM_PICTURE_BYTES 0x400000L

/* both cache formats start with 'head' and end the header with 'foot'
(cache_files.c's CACHE_FILE_HEADER_SIGNATURE, read little-endian) */
#define CACHE_HEADER_BYTES 0x800
#define CACHE_FOOTER_OFFSET 0x7FC

/* how long the client waits for an answer before giving up, and how long the
host counts a downloading client as busy after its last request */
#define SILENCE_MILLISECONDS 15000
/* while the player is deciding, the client pings the host this often so it
isn't dropped as idle */
#define KEEP_ALIVE_MILLISECONDS 5000

/* maps everyone has, which are never sent: the Xbox campaign and
multiplayer levels, Halo PC's multiplayer maps, and the resource maps */
static char const *const stock_map_names[] =
{
	"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40",
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout",
	"carousel", "boardingaction", "bloodgulch", "wizard", "putput", "longest",
	"icefields", "deathisland", "dangercanyon", "infinity", "timberland", "gephyrophobia",
	"ui", "bitmaps", "sounds", "loc",
};

/* ---------- structures */

/* client to host: asks for a file's size (offset NONE) or a piece of it */
struct map_download_request
{
	struct distributed_message_header header;
	char map_name[MAP_NAME_SIZE];
	byte file;
	byte pad[3];
	long offset;
	long length;
};

/* host to client: a file's size (NONE if the host won't send it), plus a
piece of the file when offset isn't NONE */
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

/* an answer has to fit in one network message */
typedef char map_download_answer_size_assert[sizeof(struct map_download_answer) <= 0xFFF ? 1 : -1];

/* ---------- prototypes */

/* network_game_globals.c's and network_server_message_handler.c's */
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);

/* ---------- private code */

/* only plain file names: letters, digits, spaces and _ - . with no leading
dot, no "..", no path, and not a stock map */
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

/* when each client machine last sent a request */
static unsigned long host_request_times[HALO_PORT_MAXIMUM_NETWORK_MACHINES];

/* finds the file a request is for; only the map the game is running, or the
picture next to it, is ever allowed */
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

/* answers with the file's size, or with the piece that was asked for */
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
	/* a request outside the file or bigger than REQUEST_BYTES didn't come
	from this game's client, so it gets no answer */
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
	/* the server browser's JOIN: asking whether to download before joining */
	_client_asking_join,
	/* joined, waiting for the host to send the file sizes */
	_client_sizing,
	/* joined, asking the player (when they weren't asked on JOIN) */
	_client_asking,
	_client_downloading,
	/* stopped, showing the player why */
	_client_failed,
};

static struct
{
	short state;
	/* the map's tag path from the host's game (levels\...\<name>), and the
	file name at the end of it */
	char map_name[0x80];
	char name[MAP_NAME_SIZE];
	/* each file's size from the host (NONE if it won't send it), and a flag
	per file for the sizes that have arrived */
	long sizes[NUMBER_OF_MAP_DOWNLOAD_FILES];
	short sizes_known;
	/* the file being downloaded, and how much of it has arrived and has been
	requested */
	byte file;
	long received;
	long requested;
	FILE *stream;
	unsigned long heard_time;
	unsigned long asked_time;
	char failure[128];
	/* the host started the game before the map finished downloading */
	boolean begin_held;
	/* the map the player accepted on JOIN, so the lobby doesn't ask again,
	and whether the current download is that map */
	char approved_name[MAP_NAME_SIZE];
	boolean approved;
	/* the browser's join, run once the player accepts on JOIN */
	void (*join)(void);
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

/* closes and deletes a partly downloaded file */
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

/* starts over for a new map, keeping only the map accepted on JOIN */
static void client_reset(
	char const *map_name,
	short state)
{
	char approved_name[MAP_NAME_SIZE];

	client_forget();
	csmemcpy(approved_name, client.approved_name, sizeof(approved_name));
	csmemset(&client, 0, sizeof(client));
	csmemcpy(client.approved_name, approved_name, sizeof(approved_name));
	snprintf(client.map_name, sizeof(client.map_name), "%s", map_name);
	snprintf(client.name, sizeof(client.name), "%s", tag_name_strip_path(map_name));
	client.sizes[_map_download_map] = NONE;
	client.sizes[_map_download_picture] = NONE;
	client.state = state;
}

/* the player cancelled: drop the download and leave the game */
static void client_leave(
	void)
{
	client_forget();
	network_game_abort();
}

/* keeps up to WINDOW_BYTES requested beyond what has arrived */
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

/* the map is installed: load it, and if the host already started the game,
start it here too */
static void client_done(
	void)
{
	struct network_game_client *network_client = global_network_game_client_get();
	boolean begin_held = client.begin_held;

	error(_error_silent, "map download: %s: done", client.name);
	client.state = _client_idle;
	client.begin_held = FALSE;
	custom_edition_maps_look_again();
	main_set_multiplayer_map_name(client.map_name);
	if (begin_held && network_client)
	{
		/* (despite the name, this starts the game; the begin-game message
		handler uses it the same way) */
		boolean started = network_game_client_game_has_started(network_client);

		if (!started)
			error(_error_silent, "map download: %s: couldn't start the game the host began", client.name);
	}
}

/* a file couldn't be downloaded. Without the map the download stops; without
the picture the menus just show the unknown-level picture */
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

/* whether the first piece starts like the right kind of file: a cache header
for the map, "BM" for the picture. Anything else is dropped right away */
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

/* whether the whole file really is what it should be, judged by its contents
rather than its name */
static boolean client_file_valid(
	byte file,
	char const *path)
{
	if (file == _map_download_map)
		return custom_edition_cache_file_is_map(path) || cache_files_xbox_map_playable(path);

	return client_picture_valid(path);
}

/* the whole file has arrived: keep it if it checks out, otherwise delete it.
After the map comes the picture, if the host has one */
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

/* a size from the host. Once both are in, ask the player, or start right
away if they accepted on JOIN */
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

/* a piece of the current file; anything but the next expected piece is
ignored */
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
	/* only complete answers about the map being downloaded */
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

/* runs every frame the menus do: drops the download if the connection is
gone, gives up on a host that stopped answering, and pings the host while
the player decides */
static void client_update(
	void)
{
	unsigned long now = system_milliseconds();

	/* (asking on JOIN happens before there is a connection) */
	if (client.state != _client_idle && client.state != _client_asking_join && !global_network_game_client_get())
	{
		client_forget();
	}
	else if ((client.state == _client_sizing || client.state == _client_downloading) &&
		now - client.heard_time > SILENCE_MILLISECONDS)
	{
		client_fail("The host stopped\nanswering. It may be\non a version that\ncan't send maps.");
	}
	else if (client.state == _client_asking && now - client.asked_time > KEEP_ALIVE_MILLISECONDS)
	{
		client_send_request(_map_download_map, NONE, 0);
	}
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

/* the dialog's widget (ce/map_download.xml, from tools/port_settings.py's
_map_download): the tag it's loaded by, and the name of its instance */
#define DIALOG_WIDGET "pc\\map_download\\map_download_screen"
#define DIALOG_WIDGET_NAME "map_download_screen"
/* map names longer than this are cut short to fit a line */
#define DIALOG_NAME_CHARACTERS 22

/* Glassed draws the dialog itself. Its widgets are still there underneath,
invisible, so focus and the mouse work as in Vanilla
(tools/port_settings.py's _map_download(overlay)) */
enum
{
	PANEL_X = 130, PANEL_Y = 150, PANEL_WIDTH = 380, PANEL_HEIGHT = 190,
	TEXT_Y = PANEL_Y + 52, LINE_HEIGHT = 17,
	BAR_Y = PANEL_Y + 124, BAR_HEIGHT = 8,
	BUTTONS_Y = PANEL_Y + PANEL_HEIGHT - 36, BUTTON_WIDTH = 120, BUTTON_HEIGHT = 24,
	ACCEPT_X = 190, CANCEL_X = 330,
};

/* source/interface/ui_widget.c's */
struct widget_instance *ui_widget_port_open_layer(char const *name);
void ui_widget_port_close_layer(struct widget_instance *layer);
struct widget_instance *ui_widget_port_top(void);

/* the dialog, if it's open on player 1's screen */
static struct widget_instance *dialog_find(
	void)
{
	struct widget_instance *top = ui_widget_port_top();
	struct widget_instance *child;

	for (child = top ? top->child : NULL; child; child = child->next)
	{
		if (child->name && !strcmp(child->name, DIALOG_WIDGET_NAME))
			return child;
	}

	return NULL;
}

/* finds a widget by name anywhere under `widget` */
static struct widget_instance *dialog_widget(
	struct widget_instance *widget,
	char const *name)
{
	struct widget_instance *child;
	struct widget_instance *found;

	if (!widget)
		return NULL;
	if (widget->name && !strcmp(widget->name, name))
		return widget;
	for (child = widget->child; child; child = child->next)
	{
		if ((found = dialog_widget(child, name)) != NULL)
			return found;
	}

	return NULL;
}

static void format_megabytes(
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
	if (!global_network_game_client_get())
		return FALSE;
	client_reset(map_name, _client_sizing);
	/* (accepting on JOIN covers this one download only) */
	client.approved = !csstrcasecmp(client.approved_name, client.name);
	client.approved_name[0] = 0;
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
	client_reset(map_name, _client_asking_join);
	client.approved_name[0] = 0;
	client.join = join;

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
	char name[DIALOG_NAME_CHARACTERS + 4];
	char sizes[NUMBER_OF_MAP_DOWNLOAD_FILES][24];
	char received[24];

	dialog_map_name(name, sizeof(name));
	format_megabytes(client.sizes[_map_download_map], sizes[_map_download_map], sizeof(sizes[0]));
	format_megabytes(client.sizes[_map_download_picture], sizes[_map_download_picture], sizeof(sizes[0]));
	format_megabytes(client.received, received, sizeof(received));
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
		/* accepting joins the game, and the download starts once the host
		answers; cancelling joins nothing */
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
	struct widget_instance *dialog;

	client_update();
	dialog = dialog_find();
	if (client.state != _client_idle && !dialog)
		ui_widget_port_open_layer(DIALOG_WIDGET);
	else if (client.state == _client_idle && dialog)
		ui_widget_port_close_layer(dialog);
}

boolean map_download_dialog_up(
	void)
{
	return dialog_find() != NULL;
}

/* draws one of Glassed's buttons, highlighted when it has the focus */
static void overlay_button(
	struct overlay_palette const *palette,
	struct widget_instance *bar,
	struct widget_instance *button,
	float x,
	char const *label)
{
	boolean lit = button && bar && bar->focused_child == button;

	if (!button || !button->visible || !label)
		return;
	ui_overlay_rect(x, BUTTONS_Y, BUTTON_WIDTH, BUTTON_HEIGHT, palette->radius / 2,
		lit ? palette->row_selected : palette->panel);
	ui_overlay_outline(x, BUTTONS_Y, BUTTON_WIDTH, BUTTON_HEIGHT, palette->radius / 2, 0.75f, palette->panel_edge);
	ui_overlay_text(UI_FONT_BOLD, 10.0f, x + BUTTON_WIDTH / 2, BUTTONS_Y + 6, UI_ALIGN_CENTER,
		lit ? palette->title : palette->prompt, label);
}

void map_download_overlay_render(
	void)
{
	struct overlay_palette const *palette = overlay_palette_current();
	struct widget_instance *dialog = dialog_find();
	struct widget_instance *bar = dialog_widget(dialog, "button_bar");
	float margin = (float)((halo_screen_width() - 640) / 2 + 2);
	char const *accept, *cancel;
	char text[160];
	char *line;
	float y = TEXT_Y;

	if (!dialog || !palette->glassed || !ui_overlay_available())
		return;
	map_download_dialog(text, sizeof(text), &accept, &cancel);
	ui_overlay_rect(-margin, 0, 640 + 2 * margin, 480, 0, 0x00000099);
	ui_overlay_rect(PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT, palette->radius, palette->backdrop);
	ui_overlay_outline(PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT, palette->radius, 0.75f, palette->panel_edge);
	ui_overlay_text(UI_FONT_BOLD, 18.0f, PANEL_X + 20, PANEL_Y + 16, UI_ALIGN_LEFT, palette->title, "MAP DOWNLOAD");
	ui_overlay_rect(PANEL_X + 20, PANEL_Y + 42, PANEL_WIDTH - 40, 0.75f, 0, palette->rule);
	for (line = strtok(text, "\n"); line; line = strtok(NULL, "\n"))
	{
		overlay_text_fitted(UI_FONT_REGULAR, 12.0f, PANEL_X + 20, y, PANEL_WIDTH - 40,
			client.state == _client_failed ? OVERLAY_COLOR_POOR : palette->text, line);
		y += LINE_HEIGHT;
	}
	if (client.state == _client_downloading && client.sizes[client.file] > 0)
	{
		float done = (float)client.received / (float)client.sizes[client.file];

		ui_overlay_rect(PANEL_X + 20, BAR_Y, PANEL_WIDTH - 40, BAR_HEIGHT, BAR_HEIGHT / 2, palette->panel);
		ui_overlay_rect(PANEL_X + 20, BAR_Y, (PANEL_WIDTH - 40) * done, BAR_HEIGHT, BAR_HEIGHT / 2,
			OVERLAY_COLOR_NOTICE);
	}
	overlay_button(palette, bar, dialog_widget(dialog, "button_accept"), ACCEPT_X, accept);
	overlay_button(palette, bar, dialog_widget(dialog, "button_cancel"), CANCEL_X, cancel);
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

void map_download_overlay_render(
	void)
{
}

#endif
