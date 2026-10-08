/*
NETWORK_GAME_GLOBALS.H

header included in hcex build.
*/

#ifndef __NETWORK_GAME_GLOBALS_H
#define __NETWORK_GAME_GLOBALS_H
#pragma once

/* ---------- constants */

/* game advertisement flags */
enum
{
	_game_advertisement_open_bit = 1,
	_game_advertisement_has_teams_bit,
	_game_advertisement_oddball_variant_bit
};

/* network game platforms */
enum
{
	_network_game_platform_xbox,
	_network_game_platform_pc,
	NUMBER_OF_NETWORK_GAME_PLATFORMS
};

/* network game client states */
enum
{
	_network_game_client_state_searching,
	_network_game_client_state_joining,
	_network_game_client_state_pregame,
	_network_game_client_state_ingame,
	_network_game_client_state_postgame,
	NUMBER_OF_NETWORK_GAME_CLIENT_STATES
};

/* ---------- macros */

/* ---------- structures */

struct network_game;
struct network_game_server;
struct network_game_client;
struct network_player;

/* ---------- prototypes/NETWORK_GAME_GLOBALS.C */

boolean network_game_is_active(
	void);
void network_game_set_number_of_games_played(
	long number_of_games_played);
void network_game_set_random_seed(
	long random_seed);
struct network_game *network_game_get_game(
	void);
boolean network_game_player_is_local(
	struct network_player *player);
void network_game_accept_remote_connections(
	boolean accept_remote_connections);
boolean network_game_should_accept_remote_connections(
	void);
boolean network_game_is_splitscreen_local(
	void);
void network_game_set_quickstart_local(
	void);
boolean network_game_is_quickstart_local(
	void);
struct network_game_server *global_network_game_server_get(
	void);
void dispose_global_network_game_server(
	void);
boolean network_game_server_start_frame(
	void);
struct network_game_client *global_network_game_client_get(
	void);
boolean create_global_network_game_client(
	void);
void dispose_global_network_game_client(
	void);
boolean network_game_client_start_frame(
	void);
boolean network_game_client_end_frame(
	void);
short network_game_client_get_local_machine_index(
	void);
void network_game_client_local_player_quit(
	short controller_index);
long network_game_get_number_of_games_played(
	void);
long network_game_get_random_seed(
	void);
void network_game_abort(
	void);
void network_game_client_all_local_players_have_quit(
	void);
void network_game_client_request_immediate_start(
	void);
boolean create_global_network_game_server(
	void);
/* a client of the distributed netcode (port/linux/NETCODE.md), which
decides nothing the host does */
boolean network_game_distributed_client(
	void);

/* ---------- globals */

/* ---------- public code */

#endif // __NETWORK_GAME_GLOBALS_H
