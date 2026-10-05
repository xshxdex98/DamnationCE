/*
MAP_DOWNLOAD.H

Downloads a custom map from the host when a player joins a game on a map
they don't have (map_download.c).
*/

#ifndef __MAP_DOWNLOAD_H
#define __MAP_DOWNLOAD_H

/* ---------- constants */

/* downloaded maps go in a folder of their own, which the map loaders check
after the game's maps folder (custom_edition_cache.c, cache_files.c) */
#define DOWNLOADED_MAPS_DIRECTORY "z:\\downloaded_maps\\"

/* ---------- prototypes */

/* whether this machine is missing a map (levels\...\<name>) that a host
could send */
boolean map_download_needed(
	char const *map_name);

/* browser_screen.c: the player pressed JOIN on a game whose map is missing.
Asks whether to download it; if they accept, `join` is called and the map
downloads before the lobby is entered. FALSE if the map can't be offered. */
boolean map_download_ask(
	char const *map_name,
	void (*join)(void));

/* network_client_manager.c: the host's game is on a missing map. Starts the
download, asking the player first unless they already accepted on JOIN.
Until the map is installed it isn't loaded, and this machine's players
aren't added to the game (map_download_holds_players). */
boolean map_download_begin(
	char const *map_name);
boolean map_download_holds_players(
	void);
/* network_client_message_handler.c: the host started a game while the map
is still downloading. Returns TRUE to hold the start; the game is started
once the map is installed. */
boolean map_download_hold_begin(
	void);

/* network_distributed.c: the download's messages. They're handled before
the game runs, since a client in the lobby or joining a game in progress
has no game yet. machine_index is the client's on the host, NONE on a
client. */
boolean map_download_message(
	byte type);
void map_download_handle_message(
	long machine_index,
	byte type,
	void const *data,
	word size);

/* network_server_manager.c: whether a client machine sent a download
request recently, so it isn't dropped for having no players */
boolean map_download_machine_busy(
	long machine_index);

/* ui_widget.c, every frame: opens the dialog (ce/map_download.xml) while
there's a download to show and closes it afterwards */
void map_download_menus_update(
	void);
boolean map_download_dialog_up(
	void);
/* ui_widget.c, after the menus are drawn: Glassed draws the dialog itself
over its invisible widgets. Vanilla shows the widgets as they are */
void map_download_overlay_render(
	void);
/* menu_functions.c: the dialog's text (short lines, like the stock dialogs)
and its button labels, where a NULL accept hides that button; and a press
of either button */
void map_download_dialog(
	char *text,
	size_t text_size,
	char const **accept,
	char const **cancel);
void map_download_dialog_press(
	boolean accept);

#endif
