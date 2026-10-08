/*
WEAPON_HUD_INTERFACE_DEFINITION.H
*/

#ifndef __WEAPON_HUD_INTERFACE_DEFINITION_H
#define __WEAPON_HUD_INTERFACE_DEFINITION_H
#pragma once

/* ---------- headers */

#include "interface/hud_definitions.h"

/* ---------- structures */

struct weapon_flash_state_definition
{
	short flags;
	short pad;
	short total_ammo;
	short loaded_ammo;
	short heat;
	short age;
	long unused[8];
};

struct weapon_hud_interface_definition
{
	struct tag_reference parent_hud;
	struct weapon_flash_state_definition flash_cutoffs;
	struct hud_absolute_placement_definition absolute_placement;
	struct tag_block statics;
	struct tag_block meters;
	struct tag_block numbers;
	struct tag_block crosshairs;
	struct tag_block overlays;
	unsigned long valid_crosshair_types_flags;
	struct tag_block warning_sounds;
	struct tag_block screen_effects;
	long unused1[33];
	struct icon_hud_element_definition messaging_icon;
	long unused2[12];
};

struct weapon_hud_element_header
{
	short state_type;
	short runtime_flags;
	short use_on_map_type;
	short pad;
	long unused[7];
};

typedef char weapon_hud_element_header_size_assert[
	sizeof(struct weapon_hud_element_header) == 0x24 ? 1 : -1];

struct weapon_hud_static_element
{
	struct weapon_hud_element_header header;
	struct static_hud_element_definition static_element;
	long unused[10];
};

typedef char weapon_hud_static_element_size_assert[
	sizeof(struct weapon_hud_static_element) == 0xB4 ? 1 : -1];

struct weapon_hud_meter_element
{
	struct weapon_hud_element_header header;
	struct meter_hud_element_definition meter_element;
	long unused[10];
};

typedef char weapon_hud_meter_element_size_assert[
	sizeof(struct weapon_hud_meter_element) == 0xB4 ? 1 : -1];

struct weapon_hud_number_element
{
	struct weapon_hud_element_header header;
	struct number_hud_element_definition number_element;
	word weapon_flags;
	short pad;
	long unused[9];
};

typedef char weapon_hud_number_element_size_assert[
	sizeof(struct weapon_hud_number_element) == 0xA0 ? 1 : -1];

struct weapon_hud_crosshair_item
{
	struct hud_placement_definition placement;
	struct hud_color_definition colors;
	short frame_rate;
	short sequence_index;
	unsigned long flags;
	long unused[8];
};

typedef char weapon_hud_crosshair_item_size_assert[
	sizeof(struct weapon_hud_crosshair_item) == 0x6C ? 1 : -1];

struct weapon_hud_crosshair_definition
{
	struct tag_reference bitmap;
	struct tag_block items;
};

typedef char weapon_hud_crosshair_definition_size_assert[
	sizeof(struct weapon_hud_crosshair_definition) == 0x1C ? 1 : -1];

struct weapon_hud_crosshairs_element
{
	short crosshair_type;
	short runtime_flags;
	short use_on_map_type;
	short pad;
	long unused[7];
	struct weapon_hud_crosshair_definition crosshairs;
	long unused2[10];
};

typedef char weapon_hud_crosshairs_element_size_assert[
	sizeof(struct weapon_hud_crosshairs_element) == 0x68 ? 1 : -1];

struct weapon_hud_overlays_element
{
	short state_type;
	short runtime_flags;
	short use_on_map_type;
	short pad;
	long unused[7];
	struct weapon_hud_overlay_definition overlays;
	long unused2[10];
};

typedef char weapon_hud_overlays_element_size_assert[
	sizeof(struct weapon_hud_overlays_element) == 0x68 ? 1 : -1];

#endif // __WEAPON_HUD_INTERFACE_DEFINITION_H
