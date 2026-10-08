/*
PERIODIC_FUNCTIONS.H
*/

#ifndef __PERIODIC_FUNCTIONS_H
#define __PERIODIC_FUNCTIONS_H
#pragma once

/* ---------- headers */

#include "math/real_math.h"

/* ---------- constants */

/* periodic functions */
enum
{
	_periodic_function_one,
	_periodic_function_zero,
	_periodic_function_cosine,
	_periodic_function_cosine_variable_period,
	_periodic_function_diagonal_wave,
	_periodic_function_diagonal_wave_variable_period,
	_periodic_function_slide,
	_periodic_function_slide_variable_period,
	_periodic_function_noise,
	_periodic_function_jitter,
	_periodic_function_wander,
	_periodic_function_spark,
	NUMBER_OF_PERIODIC_FUNCTIONS
};

enum
{
	_transition_function_linear = 0,
	_transition_function_early,
	_transition_function_very_early,
	_transition_function_late,
	_transition_function_very_late,
	_transition_function_cosine,
	NUMBER_OF_TRANSITION_FUNCTIONS,
};

/* ---------- prototypes/PERIODIC_FUNCTIONS.C */

real periodic_function_evaluate(
	short function_type,
	real time);
real transition_function_evaluate(
	short function_type,
	real value);
void periodic_functions_initialize(
	void);
void periodic_functions_dispose(
	void);

#endif // __PERIODIC_FUNCTIONS_H
