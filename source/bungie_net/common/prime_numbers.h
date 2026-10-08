/*
PRIME_NUMBERS.H
*/

#ifndef __PRIME_NUMBERS_H
#define __PRIME_NUMBERS_H
#pragma once

/* ---------- structures */

struct qword_value;

/* ---------- prototypes/PRIME_NUMBERS.C */

unsigned long randomprime(
	unsigned long maximum);

void probable_prime64(
	struct qword_value *result);

#endif // __PRIME_NUMBERS_H
