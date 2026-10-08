/*
DAMAGE.H
*/

#ifndef __DAMAGE_H
#define __DAMAGE_H
#pragma once

/* ---------- headers */

#include "objects.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* damage categories */
enum
{
	_damage_category_none,
	_damage_category_falling,
	_damage_category_bullet,
	_damage_category_grenade,
	_damage_category_highexplosive,
	_damage_category_sniper,
	_damage_category_melee,
	_damage_category_flame,
	_damage_category_mountedweapon,
	_damage_category_vehicle,
	_damage_category_plasma,
	_damage_category_needle,
	_damage_category_shotgun,
	NUMBER_OF_DAMAGE_CATEGORIES
};

enum
{
	_object_being_damaged_body_depleted_bit = 0,
	_object_being_damaged_region_destroyed_bit,
	_object_being_damaged_body_destroyed_bit,
	_object_being_damaged_shield_depleted_bit,
	_object_being_damaged_by_friendly_bit,
	_object_being_damaged_multiplied_by_difficulty_bit,
	_object_being_damaged_killed_instantly_bit,
	_object_being_damaged_force_hard_ping_bit,
	NUMBER_OF_OBJECT_BEING_DAMAGED_FLAGS,
};

enum
{
	_damage_area_of_effect_bit = 0,
	_damage_create_localized_effect_bit,
	_damage_kill_instantly_bit,
	_damage_from_weapon_bit,
	_damage_silent_bit,
	_damage_bypasses_shields_bit,
	_damage_damaged_one_object_bit,
	_damage_no_statistics_bit,
	NUMBER_OF_DAMAGE_DATA_FLAGS,
};

/* ---------- macros */

/* ---------- structures */

struct damage_region
{
	char name[TAG_STRING_LENGTH+1];
	unsigned long flags;
	long unused0;
	real damage_threshold;
	long unused1[3];
	struct tag_reference destroyed_effect;
	struct tag_block permutations;
};

struct damage_data
{
	long definition_index;
	unsigned long flags;
	long owner_player_index;
	long owner_object_index;
	short owner_team_index;
	struct location location;
	real_point3d origin;
	real_point3d epicenter;
	real_vector3d direction;
	real scale;
	real multiplier;
	real material_effect_scale;
	short material_type;
	struct projectile_material_response_definition const *material_response;
};

/* an object's vitality and recent damage, as the distributed netcode's host
sends them (port/linux/game/network_distributed.c) */
struct damage_network_state
{
	boolean shield_depleted;
	boolean shield_charging;
	boolean shield_over_charging;
	real body_vitality;
	real shield_vitality;
	real current_body_damage;
	real recent_body_damage;
	real current_shield_damage;
	real recent_shield_damage;
};

/* ---------- prototypes/DAMAGE.C */

void damage_initialize(void);
void damage_set_network_state(long object_index, struct damage_network_state const *state);
void damage_get_network_state(long object_index, struct damage_network_state *state);
void damage_dispose(void);
void damage_initialize_for_new_map(void);
void damage_dispose_from_old_map(void);
void damage_render_debug(void);
void render_debug_object_damage(
	void);
void object_initialize_vitality(long object_index, real *custom_body_vitality, real *custom_shield_vitality);
void object_can_take_damage(long object_list_index);
void object_cannot_take_damage(long object_list_index);
void object_set_ranged_attack_inhibited(long object_index, boolean inhibited);
void object_set_melee_attack_inhibited(long object_index, boolean inhibited);
real object_get_actual_body_vitality(long object_index, boolean ignore_difficulty);
real object_get_actual_shield_vitality(long object_index, boolean ignore_difficulty);
real object_get_maximum_body_vitality(long object_index, boolean ignore_difficulty);
real object_get_maximum_shield_vitality(long object_index, boolean ignore_difficulty);

void object_damage_update(
	long object_index);
void object_destroy(
	long object_index);
void damage_data_new(struct damage_data *damage_data, long definition_index);
boolean object_restore_body(long object_index);
boolean object_double_charge_shield(long object_index);

void object_deplete_shield(
	long object_index);

void object_deplete_body(
	long object_index);
void area_of_effect_cause_damage(
	struct damage_data *damage,
	long unused_object_index);

void object_cause_damage(
	struct damage_data *damage,
	long object_index,
	short node_index,
	short region_index,
	short material_index,
	real_vector3d const *object_normal);

/* ---------- globals */

/* ---------- public code */

#ifdef HALO_64BIT
void render_debug_object_damage(
	void);

#endif
#endif // __DAMAGE_H
