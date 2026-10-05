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

/* whether this machine lacks a map (levels\...\<name>) a host can send */
boolean map_download_needed(
	char const *map_name);

/* browser_screen.c: the player picked a game on such a map. Asks whether to
download it and join; on yes, join is called, and the map is fetched before
the lobby is joined. FALSE if it can't be asked. */
boolean map_download_ask(
	char const *map_name,
	void (*join)(void));

/* network_client_manager.c: the host's game is on such a map: fetched (the
player asked first unless map_download_ask did). While it is offered or
downloading, the map isn't loaded and this machine's players aren't added
to the game (map_download_holds_players); once it is here, they are. */
boolean map_download_begin(
	char const *map_name);
boolean map_download_holds_players(
	void);
/* network_client_message_handler.c: the host began the game (one in
progress, which doesn't wait for this machine's map as a lobby's start
does). TRUE if the map is still being offered or downloaded: the game is
then begun once the map is here. */
boolean map_download_hold_begin(
	void);

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

/* network_server_manager.c: whether a client machine is downloading the
map (it asked for some of it lately), so isn't idle for having no players */
boolean map_download_machine_busy(
	long machine_index);

/* The dialog that asks the player and shows the download, a stock one of
the menus' (ce/map_download.xml). ui_widget.c, each frame: opens it while
there is a download to show, and closes it after; whether it is up. */
void map_download_menus_update(
	void);
boolean map_download_dialog_up(
	void);
/* menu_functions.c: its text (the stock dialogs' short lines) and its
buttons' labels (accept NULL: that button hidden), each frame it's up; its
buttons pressed */
void map_download_dialog(
	char *text,
	size_t text_size,
	char const **accept,
	char const **cancel);
void map_download_dialog_press(
	boolean accept);

#endif
