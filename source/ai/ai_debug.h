/*
AI_DEBUG.H
*/

#ifndef __AI_DEBUG_H
#define __AI_DEBUG_H
#pragma once

/* ---------- headers */

#include "actors.h"

#include "math/real_math.h"

/* ---------- constants */

enum
{
	MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS = 16,
	MAXIMUM_AI_DEBUG_IDLE_LOOK_PROPS = 32,
	NUMBER_OF_AI_DEBUG_COMMUNICATION_TYPES = 57,
	NUMBER_OF_AI_DEBUG_VOCALIZATION_TYPES = 209,
	MAXIMUM_AI_DEBUG_LINEOFSIGHT_POINTS = 16384,
	MAXIMUM_AI_DEBUG_LINEOFSIGHT_PAIRS = 8192,
	NUMBER_OF_AI_DEBUG_ACTOR_RECORDS = 512,
	MAXIMUM_AI_DEBUG_PATH_STORAGE = 32,
};

enum
{
	_firing_disabled = 0,
	_firing_busy,
	_firing_wrong_target,
	_firing_no_target,
	_firing_outside_active_area,
	_firing_not_visible,
	_firing_outside_range,
	_firing_blocked,
	_firing_holding_for_line,
	_firing_holding,
	_firing_pausing_for_line,
	_firing_pausing,
	_firing_wild,
	_firing_burst,
	_firing_not_in_midair,
	_firing_not_crouching,
	_firing_not_standing,
	_firing_not_stationary,
	_firing_underwater,
	_firing_min_range,
	NUMBER_OF_ACTOR_DEBUG_FIRING_DECISIONS,
};

enum
{
	_grenade_vehicle = 0,
	_grenade_unit_busy,
	_grenade_being_hurt,
	_grenade_no_grenades,
	_grenade_random_failed,
	_grenade_encounter_timeout,
	_grenade_target_failed,
	_grenade_not_enough_enemies,
	_grenade_collateral_damage,
	_grenade_trajectory_failed,
	_grenade_success,
	NUMBER_OF_ACTOR_DEBUG_GRENADE_DECISIONS,
};

enum
{
	_danger_avoidance_none = 0,
	_danger_avoidance_unnoticed,
	_danger_avoidance_animation_busy,
	_danger_avoidance_vehicle,
	_danger_avoidance_far_away,
	_danger_avoidance_outside_zone,
	_danger_avoidance_evasion_disallowed,
	_danger_avoidance_no_safe_direction,
	_danger_avoidance_no_desire,
	_danger_avoidance_can_avoid,
	_danger_avoidance_imminent_explosion,
	_danger_avoidance_imminent_impact,
	_danger_avoidance_no_animation,
	_danger_avoidance_attached_to_us,
	NUMBER_OF_ACTOR_DEBUG_DANGER_AVOIDANCE_DECISIONS,
};

enum
{
	_dive_not_attempted = 0,
	_dive_cannot_move,
	_dive_no_animation,
	_dive_animation_failure,
	_dive_success,
	NUMBER_OF_ACTOR_DEBUG_DIVE_DECISIONS
};

enum
{
	_charge_vehicle_success = 0,
	_charge_vehicle_not_driver,
	_charge_melee_swarm_cant,
	_charge_melee_inhibited,
	_charge_melee_notarget,
	_charge_melee_no_animation,
	_charge_melee_cannot_move,
	_charge_melee_success,
	_charge_stalking_success,
	_charge_close_success,
	NUMBER_OF_ACTOR_DEBUG_CHARGE_DECISIONS
};

/* ---------- structures */

/* one record per encounter firing position, filled by actor_select_firing_position */
struct ai_debug_actor_record
{
	boolean pursuit;
	boolean valid;
	struct firing_position firing_position;
};

struct ai_debug_lineofsight_pair
{
	short start_index;
	short end_index;
	short reference_count;
};

struct ai_debug_state
{
	boolean enter_debugger;
	boolean select_this_actor;
	boolean fix_defending_guard_firing_positions;
	boolean fix_actor_variants;
	boolean fast_los;
	boolean evaluate_all_positions;
	boolean ignore_player;
	boolean invisible_player;
	boolean flee_always;
	boolean force_all_active;
	boolean disable_wounded_sounds;
	boolean blind;
	boolean deaf;
	boolean force_vocalizations;
	boolean force_crouch;
	boolean path_disable_obstacle_avoidance;
	boolean path_disable_smoothing;
	boolean oversteer_disable;
	char selected_squad_name[32];
	int selected_squad_index;
	int selected_actor_index;
	boolean path;
	boolean path_start_freeze;
	boolean path_end_freeze;
	boolean path_flood;
	real path_maximum_radius;
	boolean path_attractor;
	real path_attractor_radius;
	real path_attractor_weight;
	real path_accept_radius;
	unsigned long communication_suppress_vector[BIT_VECTOR_SIZE_IN_LONGS(NUMBER_OF_AI_DEBUG_COMMUNICATION_TYPES)];
	unsigned long communication_ignore_vector[BIT_VECTOR_SIZE_IN_LONGS(NUMBER_OF_AI_DEBUG_COMMUNICATION_TYPES)];
	unsigned long communication_focus_vector[BIT_VECTOR_SIZE_IN_LONGS(NUMBER_OF_AI_DEBUG_VOCALIZATION_TYPES)];
	boolean communication_focus_enable;
	boolean communication_random_disabled;
	boolean communication_timeout_disabled;
	boolean communication_unit_repeat_disabled;
	boolean ballistic_lineoffire_freeze;
	boolean print_major_upgrade;
	boolean print_pursuit_checks;
	boolean print_rules;
	boolean print_rule_values;
	boolean print_respawn;
	boolean print_evaluation_statistics;
	boolean print_communication;
	boolean print_communication_player;
	boolean print_vocalizations;
	boolean print_placement;
	boolean print_speech;
	boolean print_speech_timers;
	boolean print_allegiance;
	boolean print_lost_speech;
	boolean print_migration;
	boolean print_automatic_migration;
	boolean print_scripting;
	boolean print_surprise;
	boolean print_command_lists;
	boolean print_damage_modifiers;
	boolean print_secondary_looking;
	boolean print_oversteer;
	boolean print_conversations;
	boolean print_killing_sprees;
	boolean print_acknowledgement;
	boolean print_unfinished_paths;
	boolean print_bsp_transition;
	boolean print_uncovering;
	boolean render;
	boolean render_all_actors;
	boolean render_inactive_actors;
	boolean render_lineoffire_crouching;
	boolean render_lineoffire;
	boolean render_lineofsight;
	boolean render_ballistic_lineoffire;
	boolean render_encounter_activeregion;
	boolean render_vision_cones;
	boolean render_current_state;
	boolean render_detailed_state;
	boolean render_props;
	boolean render_props_web;
	boolean render_props_no_friends;
	boolean render_props_unreachable;
	boolean render_props_unopposable;
	boolean render_props_target_weight;
	boolean render_idle_look;
	boolean render_targets;
	boolean render_targets_last_visible;
	boolean render_states;
	boolean render_support_surfaces;
	boolean render_recent_damage;
	boolean render_threats;
	boolean render_emotions;
	boolean render_audibility;
	boolean render_aiming_vectors;
	boolean render_secondary_looking;
	boolean render_vitality;
	boolean render_active_cover_seeking;
	boolean render_evaluations;
	boolean render_pursuit;
	boolean render_shooting;
	boolean render_trigger;
	boolean render_projectile_aiming;
	boolean render_aiming_validity;
	boolean render_speech;
	boolean render_teams;
	boolean render_player_ratings;
	boolean render_spatial_effects;
	boolean render_firing_positions;
	boolean render_gun_positions;
	boolean render_burst_geometry;
	boolean render_vehicle_avoidance;
	boolean render_vehicles_enterable;
	boolean render_melee_check;
	boolean render_dialogue_variants;
	boolean render_grenade_decisions;
	boolean render_danger_zones;
	boolean render_charge_decisions;
	boolean render_control;
	boolean render_activation;
	boolean render_paths;
	boolean render_paths_selected_only;
	boolean render_paths_failed;
	boolean render_paths_current;
	boolean render_paths_raw;
	boolean render_paths_smoothed;
	boolean render_paths_avoided;
	short render_paths_avoidance_segment;
	boolean render_paths_avoidance_obstacles;
	boolean render_paths_avoidance_search;
	boolean render_paths_destination;
	boolean render_paths_nodes;
	boolean render_paths_nodes_all;
	boolean render_paths_nodes_polygons;
	boolean render_paths_nodes_costs;
	boolean render_paths_nodes_closest;
	boolean render_player_aiming_blocked;
	boolean render_vector_avoidance;
	boolean render_vector_avoidance_rays;
	boolean render_vector_avoidance_sense_t;
	boolean render_vector_avoidance_avoid_t;
	boolean render_vector_avoidance_clear_time;
	boolean render_vector_avoidance_weights;
	boolean render_vector_avoidance_objects;
	boolean render_vector_avoidance_intermediate;
	boolean render_postcombat;
	long last_render_id;
	boolean lineoffire_valid;
	boolean lineoffire_success;
	real_point3d lineoffire_start;
	real_vector3d lineoffire_vector;
	long lineoffire_pill_count;
	boolean lineoffire_pill_hit[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	real_point3d lineoffire_pill_start[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	real_vector3d lineoffire_pill_vector[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	real lineoffire_pill_radius[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	boolean lineofsight_overflowed;
	long lineofsight_point_count;
	real_point3d lineofsight_point[MAXIMUM_AI_DEBUG_LINEOFSIGHT_POINTS];
	short lineofsight_point_reference_count[MAXIMUM_AI_DEBUG_LINEOFSIGHT_POINTS];
	short lineofsight_point_key[MAXIMUM_AI_DEBUG_LINEOFSIGHT_POINTS];
	long lineofsight_pair_count;
	struct ai_debug_lineofsight_pair lineofsight_pair[MAXIMUM_AI_DEBUG_LINEOFSIGHT_PAIRS];
	boolean ballistic_lineoffire_valid;
	boolean ballistic_lineoffire_success;
	real_point3d ballistic_lineoffire_start;
	real_vector3d ballistic_lineoffire_vector;
	long ballistic_lineoffire_pill_count;
	real_point3d ballistic_lineoffire_pill_start[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	real_vector3d ballistic_lineoffire_pill_end[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	real ballistic_lineoffire_pill_radius[MAXIMUM_AI_DEBUG_LINEOFFIRE_PILLS];
	long ballistic_lineoffire_point_count;
	/* (ai.c fills at most MAXIMUM_AI_DEBUG_BALLISTIC_POINTS) */
	real_point3d ballistic_lineoffire_point[64];
	boolean path_start_valid;
	real_point3d path_start_point;
	long path_start_surface_index;
	long path_start_unit_index;
	boolean path_end_valid;
	real_point3d path_end_point;
	long path_end_surface_index;
	struct path_state path_state;
	struct path_result path_result;
	struct path_debug_storage path_storage;
	boolean evaluation_context_valid;
	struct firing_position_evaluation_context evaluation_context;
	struct ai_debug_actor_record actor_record[NUMBER_OF_AI_DEBUG_ACTOR_RECORDS];
	long look_test_actor_index;
	boolean look_test_looking_valid;
	boolean look_test_aiming_valid;
	real_vector3d look_test_looking_vector;
	real_vector3d look_test_aiming_vector;
	boolean idle_look_valid;
	long idle_look_unit_index;
	short idle_look_prop_count;
	long idle_look_prop_index[MAXIMUM_AI_DEBUG_IDLE_LOOK_PROPS];
	real idle_look_prop_weight[MAXIMUM_AI_DEBUG_IDLE_LOOK_PROPS];
	boolean speak_valid;
	boolean speak_single;
	boolean speak_all;
	long speak_unit_index;
	short speak_delay_ticks;
	short speak_vocalization_type;
};

struct actor_debug_info
{
	long last_render_id;
	long last_path_refresh;
	short firing_decision;
	real shooting_rof;
	real_point3d burst_last_known_position;
	real_point3d burst_tracked_position;
	real_vector3d burst_lead_vector;
	struct
	{
		long time;
		boolean aligned;
		boolean aligned_immediately;
		real_vector3d weapon_vector;
		real_vector3d aim_vector;
		real threshold;
		real alignment;
	} burst_alignment;
	long last_projectile_aiming_time;
	boolean burst_aim_by_vector;
	real_point3d burst_origin;
	real_vector3d burst_vector;
	real_point3d burst_target;
	boolean burst_weapon_vector_valid;
	real_vector3d burst_requested_vector;
	real_vector3d burst_weapon_vector;
	boolean audibility_valid;
	short audibility_result;
	real audibility_maximum_distance;
	real audibility_distance;
	real audibility_encoded_distance;
	real audibility_audible_distance;
	boolean cover_seeking_valid;
	short cover_seeking_decision;
	short cover_seeking_target_hidden_ticks;
	real cover_seeking_shield_vitality;
	long last_vehicle_avoidance_time;
	real_point3d vehicle_avoidance_start;
	real_point3d vehicle_avoidance_center;
	real vehicle_avoidance_radius;
	real_point3d vehicle_avoidance_target;
	real vehicle_avoidance_t;
	boolean vehicle_avoidance_modified;
	real_point3d vehicle_avoidance_destination;
	long last_melee_time;
	real_point3d melee_body_point;
	real_vector3d melee_facing;
	real_point3d melee_target_point;
	real_vector3d melee_direction;
	boolean melee;
	boolean melee_in_reach;
	real_point3d melee_lead_point;
	real melee_range_lower_bound;
	real melee_range_upper_bound;
	long grenade_eval_time;
	short grenade_decision;
	short grenade_encounter_timeout_ticks;
	short grenade_enemy_count;
	short grenade_required_enemy_count;
	real grenade_current_damage;
	real grenade_random_value;
	real grenade_random_chance;
	long danger_avoidance_time;
	short danger_decision;
	boolean danger_abandoned_path;
	real danger_far_dist;
	real danger_far_radius;
	real danger_zone_dist;
	real danger_zone_radius;
	real danger_intersect_time;
	long dive_decision_time;
	short dive_decision;
	long charge_last_time;
	short charge_decision;
	real charge_move_range;
	boolean charge_leap;
	short flying_error_ticks;
	long vector_avoidance_time;
	struct vector_avoidance_data avoidance_data;
	short avoidance_type[ACTOR_MAXIMUM_AVOIDANCE_RAYS];
	real collision_t[ACTOR_MAXIMUM_AVOIDANCE_RAYS];
	real_point3d ray_origin[ACTOR_MAXIMUM_AVOIDANCE_RAYS];
	real_vector3d ray_direction[ACTOR_MAXIMUM_AVOIDANCE_RAYS];
	short avoid_ray_result[8][2];
	real avoid_t[8][2];
	real_point3d avoid_ray_origin[8][2];
	real_vector3d avoid_ray_direction[8][2];
	real avoidance_weights[8];
	real best_avoidance_weight;
	short best_avoidance_direction;
	real movement_direction_approximation;
	real movement_approximate_weight;
	real sign_no_danger;
	real forward_dot;
	real sign_too_far_cosangle;
	real sign_rotated;
	real maximum_sense_emergency;
	real rotation_angle;
	real_vector3d avoidance_forward;
	real_vector3d movement_direction;
	short emergency_decision;
	real_vector3d emergency_rotation;
	real emergency;
	boolean avoidance_direction_chosen;
	boolean velocity_valid;
	real velocity_weight;
	real angular_speed;
	real_vector3d avoidance_vector;
	real velocity_approximate_weight;
	long vision_last_time;
	real vision_last_maximum_distance;
	real vision_last_perception_factor;
	short perception_awareness_speed;
	short firing_position_type_mismatch_ticks;
};

/* ---------- prototypes/AI_DEBUG.C */

void ai_debug_initialize(
	void);

void ai_debug_dispose(
	void);

void ai_debug_dispose_from_old_map(
	void);

void ai_debug_clear_storage(
	void);

void ai_debug_actor_deleted(
	long actor_index);

void ai_debug_initialize_for_new_map(
	void);

void ai_debug_update(
	void);

void ai_debug_render(
	void);

// hs.c declares this as (long, char const *); the object shows the actor comes
// from ai_debug.selected_actor_index and both parameters are names
void ai_debug_vocalize(
	char const *priority_name,
	char const *vocalization_name);
void ai_debug_speak_list(
	char const *list_name);

void ai_debug_change_selected_encounter(
	boolean a1);
void ai_debug_teleport_to(
	long ai_index);

void ai_debug_change_selected_actor(
	boolean a1);

void ai_debug_select_encounter(
	long encounter_index);
void ai_debug_select_actor(
	long encounter_index,
	long actor_index);

char *ai_debug_describe_actor(
	long actor_index,
	long unit_index,
	boolean include_squad,
	char *buffer,
	long bufsize);

struct path_debug_storage *ai_debug_get_last_path(
	long actor_index);

struct path_debug_storage *ai_debug_get_path_storage(
	long actor_index);

void ai_debug_sound_point_set(
	void);

void ai_debug_lineoffire_new(
	real_point3d const *origin,
	real_vector3d const *vector);

void ai_debug_lineoffire_addpill(
	real_point3d const *base,
	real_vector3d const *directedheight,
	real width,
	boolean hit);

void ai_debug_lineoffire_success(
	boolean success);

boolean ai_debug_highlight_cluster(
	short index,
	real_argb_color const **highlight_color);

void ai_debug_lineofsight_reset(
	void);
void ai_debug_lineofsight(
	real_point3d const *start,
	short start_key,
	real_point3d const *end,
	short end_key);

void ai_debug_idle_look_clear(
	long unit_index);

void ai_debug_idle_look_addprop(
	long prop_index,
	real weight);

void ai_debug_speak(
	char const *vocalization_type_name);

/* ---------- globals */

extern struct ai_debug_state ai_debug;
extern struct actor_debug_info *actor_debug_array;
extern struct path_debug_storage *actor_path_debug_array;

extern real_point3d global_ai_debug_drawstack_next_position;
extern real_point3d global_ai_debug_drawstack_last_position;
extern real global_ai_debug_drawstack_height;
extern short global_ai_debug_string_position;
extern real_argb_color global_temporary_render_color;

#endif // __AI_DEBUG_H
