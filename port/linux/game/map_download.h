/*
MAP_DOWNLOAD.H

A client joining a game on a custom map it doesn't have gets the map from
the host (map_download.c).
*/

#ifndef __MAP_DOWNLOAD_H
#define __MAP_DOWNLOAD_H

/* ---------- constants */

/* where downloaded maps are kept: a folder of the player's own, which the
map loaders look in after the game's (custom_edition_cache.c, cache_files.c) */
#define DOWNLOADED_MAPS_DIRECTORY "z:\\downloaded_maps\\"

/* ---------- prototypes */

/* network_client_manager.c: whether this machine lacks the host's map
(levels\...\<name>) and should be offered it, and the offer. While it is
offered or downloading, the map isn't loaded; once it is here, it is. */
boolean map_download_needed(
	char const *map_name);
boolean map_download_begin(
	char const *map_name);

/* network_distributed.c: the download's messages, which pass before the game
runs (a client in the lobby, or joining one in progress, has no game yet).
machine_index is the client's on the host, NONE on a client. */
boolean map_download_message(
	byte type);
void map_download_handle_message(
	long machine_index,
	byte type,
	void const *data,
	word size);

/* the screen that asks the player and shows the download, over the menus
(ui_widget.c) */
boolean map_download_screen_active(
	void);
void map_download_screen_process(
	void);
void map_download_screen_render(
	void);
struct halo_ui_pointer;
void map_download_screen_pointer(
	struct halo_ui_pointer const *pointer);

#endif
