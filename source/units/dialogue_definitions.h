/*
DIALOGUE_DEFINITIONS.H
*/

#ifndef __DIALOGUE_DEFINITIONS_H
#define __DIALOGUE_DEFINITIONS_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

enum
{
	DIALOGUE_DEFINITION_TAG = 'udlg',
	NUMBER_OF_DIALOGUE_VOCALIZATION_TYPES = 209,
};

/* ---------- macros */

#define dialogue_definition_get(index) \
	((struct dialogue_definition *)tag_get(DIALOGUE_DEFINITION_TAG, (index)))

/* ---------- structures */

struct dialogue_definition
{
	short vocalization_enum_version;
	word pad;
	long unused[3];
	struct tag_reference vocalizations[NUMBER_OF_DIALOGUE_VOCALIZATION_TYPES];
	struct tag_reference unused_vocalizations[47];
};

typedef char dialogue_definition_size_assert[
	sizeof(struct dialogue_definition) == 0x1010 ? 1 : -1];

/* ---------- prototypes/DIALOGUE_DEFINITIONS.C */

char const *dialogue_get_vocalization_name(
	short vocalization_type,
	boolean abbreviated);
short dialogue_get_vocalization_type_by_name(
	char const *name);

/* ---------- prototypes/UNIT_DIALOGUE.C */

void unit_dialogue_determine_variant(
	long unit_index);

#endif // __DIALOGUE_DEFINITIONS_H
