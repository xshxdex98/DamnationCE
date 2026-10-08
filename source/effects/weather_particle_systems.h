/*
WEATHER_PARTICLE_SYSTEMS.H
*/

#ifndef __WEATHER_PARTICLE_SYSTEMS_H
#define __WEATHER_PARTICLE_SYSTEMS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "memory/data.h"
#include "math/real_math.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct structure_weather_polyhedron
{
	real_point3d bounding_sphere_center;
	real bounding_sphere_radius;
	long unused;
	struct tag_block planes;
};

struct structure_weather_palette_entry
{
	char name[32];
	struct tag_reference particle_system;
	word pad30;
	short runtime_particle_system_global_function_index;
	char particle_system_global_function_name[32];
	long particle_system_unused[11];
	struct tag_reference wind;
	real_vector3d wind_direction;
	real wind_magnitude;
	word padA0;
	short wind_global_function_index;
	char wind_global_function_name[32];
	long wind_unused[11];
};

/* ---------- prototypes/WEATHER_PARTICLE_SYSTEMS.C */

void weather_particle_system_new(
	short local_player_index,
	long definition_index,
	real scale);
void weather_particle_systems_render(
	void);

/* ---------- globals */

/* ---------- public code */

void weather_particle_systems_initialize(
	void);
void weather_particle_systems_initialize_for_new_map(
	void);
void weather_particle_systems_dispose_from_old_map(
	void);
void weather_particle_systems_dispose(
	void);
void weather_particle_system_delete(
	short local_player_index);

extern struct data_array *weather_particle_data;
extern boolean weather;

#endif // __WEATHER_PARTICLE_SYSTEMS_H
