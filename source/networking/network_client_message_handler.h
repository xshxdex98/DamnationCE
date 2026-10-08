/*
NETWORK_CLIENT_MESSAGE_HANDLER.H
*/

#ifndef __NETWORK_CLIENT_MESSAGE_HANDLER_H
#define __NETWORK_CLIENT_MESSAGE_HANDLER_H
#pragma once

/* ---------- headers */

#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- structures */

struct message_server_graceful_game_exit_postgame
{
	long unused;
};

struct message_server_switch_to_pregame
{
	long unused;
};

struct message_server_game_over
{
	long unused;
};

struct message_server_graceful_game_exit_pregame
{
	long unused;
};

struct message_server_begin_game
{
	long unused;
};

struct message_server_postgame_keep_alive
{
	short unused;
};

struct message_server_pregame_keep_alive
{
	short unused;
};

struct message_server_pong
{
	long timestamp;
};

struct message_server_game_settings_update
{
	word total_size;
	word offset;
	word length;
	word pad;
	byte data[HALO_PORT_NETWORK_GAME_SETTINGS_FRAGMENT_SIZE];
};

struct network_game_client;
struct transport_address;

/* ---------- prototypes/NETWORK_CLIENT_MESSAGE_HANDLER.C */

boolean network_game_client_handle_message(
	struct network_game_client *client,
	word *message,
	short message_size,
	struct transport_address *source_address);

#endif // __NETWORK_CLIENT_MESSAGE_HANDLER_H
