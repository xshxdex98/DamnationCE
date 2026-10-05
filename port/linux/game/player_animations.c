/*
PLAYER_ANIMATIONS.C

The animation graph a network co-op player's Elite uses. The campaign's own
Elite graph is the AI's and animates only the Covenant's weapons; this one
is a community multiplayer Elite graph from Halo PC's modding scene
(2005-2010, its author no longer around), made for the stock Elite's
skeleton, with every player weapon and vehicle seat. It is carried in the
game (port/assets/animations/elite_player.antr, converted from the editing
kit's tag by tools/player_animations.py) so that nobody has to install it.

When a map's tags load and the map has the stock Elite, the graph is copied
into memory the game's tags can point into, its addresses are fixed up, its
sound references are looked up among the map's tags, and it is added to the
tag table as one more animation graph tag, as menu_tags.c adds the menus'.
tag_get and the animation code then take it as any other graph. It is
removed when the map unloads. Every machine adds it the same way, so its
tag index is the same everywhere.

units.c gives it to a co-op player's Elite (unit_animation_graph_index), and
to nothing else. On a map whose Elite the graph doesn't fit (a custom one),
the player's Elite falls back to the Spartan's animations, carried over to
its skeleton (model_animations.c).
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "tag_files/tag_groups.h"
#include "models/model_animation_definitions.h"
#include "models/model_definitions.h"
#include "units/biped_definitions.h"

#include "../src/animation_files.h"

#include <stdlib.h>
#include <string.h>

/* ---------- constants */

#define ELITE_GRAPH_FILE "elite_player.antr"
#define ELITE_GRAPH_NAME "characters\\elite\\elite_player"
#define ELITE_NAME "characters\\elite\\elite"

/* the converted file (tools/player_animations.py) */
#define GRAPH_FILE_VERSION 1
enum
{
	GRAPH_FILE_HEADER_BYTES = 20,
	GRAPH_FILE_REFERENCE_HEADER_BYTES = 12,
};

/* ---------- structures */

/* (as cache_files.c has it) */
struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

typedef char verify_cache_file_tag_instance_size[
	sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];

/* ---------- prototypes */

void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);

/* ---------- globals */

static struct
{
	/* the graph's tag index, or NONE when it isn't loaded */
	long tag_index;
	/* the map's Elite biped, which the graph is for */
	long elite_definition_index;
	/* the graph and its tags' names, and the table it was added to */
	byte *graph;
	struct cache_file_tag_instance *instances;
	struct cache_file_tag_instance *original_instances;
	long original_count;
} player_animations = { NONE, NONE };

/* ---------- private code */

static struct animation_file_embedded const *graph_file_find(
	void)
{
	unsigned int index;

	for (index = 0; index < animation_files_embedded_count; index++)
	{
		if (animation_files_embedded[index].name && !strcmp(animation_files_embedded[index].name, ELITE_GRAPH_FILE))
			return &animation_files_embedded[index];
	}
	return NULL;
}

static unsigned long read_u32(
	byte const *at)
{
	unsigned long value;

	memcpy(&value, at, sizeof(value));
	return value;
}

/* The graph laid out in its own memory, its addresses and references fixed
up, or NULL if the file isn't one this reads. The memory is one block: the
graph, then its references' names. */
static byte *graph_build(
	struct animation_file_embedded const *file)
{
	byte const *bytes = (byte const *)file->data;
	unsigned long data_size, relocation_count, reference_count, index;
	unsigned long names_size = 0;
	byte const *relocations, *references, *at;
	byte *graph;
	char *names;

	if (file->size < GRAPH_FILE_HEADER_BYTES || memcmp(bytes, "antr", 4) ||
		read_u32(bytes + 4) != GRAPH_FILE_VERSION)
	{
		return NULL;
	}
	data_size = read_u32(bytes + 8);
	relocation_count = read_u32(bytes + 12);
	reference_count = read_u32(bytes + 16);
	relocations = bytes + GRAPH_FILE_HEADER_BYTES + data_size;
	references = relocations + relocation_count * 4;
	if (references > bytes + file->size)
		return NULL;
	for (at = references, index = 0; index < reference_count; index++)
	{
		unsigned long name_size = read_u32(at + 8);

		names_size += name_size;
		at += GRAPH_FILE_REFERENCE_HEADER_BYTES + name_size;
		if (at > bytes + file->size)
			return NULL;
	}

	graph = malloc(data_size + names_size);
	if (!graph)
		return NULL;
	memcpy(graph, bytes + GRAPH_FILE_HEADER_BYTES, data_size);
	/* each address field holds the offset it points to plus one, 0 for none */
	for (index = 0; index < relocation_count; index++)
	{
		unsigned long field = read_u32(relocations + index * 4);
		unsigned long target = read_u32(graph + field);

		*(XPTR(void) *)(graph + field) = target ? XBOX_ADDRESS(graph + target - 1) : XBOX_NULL;
	}
	/* each tag reference: the map's tag of that name, or none */
	names = (char *)graph + data_size;
	for (at = references, index = 0; index < reference_count; index++)
	{
		struct tag_reference *reference = (struct tag_reference *)(graph + read_u32(at));
		unsigned long name_size = read_u32(at + 8);

		memcpy(names, at + GRAPH_FILE_REFERENCE_HEADER_BYTES, name_size);
		reference->name = XBOX_ADDRESS(names);
		reference->index = tag_loaded(reference->group_tag, names);
		names += name_size;
		at += GRAPH_FILE_REFERENCE_HEADER_BYTES + name_size;
	}
	return graph;
}

/* whether the graph was made for the map's Elite: the same nodes */
static boolean graph_fits_elite(
	struct animation_graph *graph)
{
	long model_index = tag_loaded(MODELS_GROUP_TAG, ELITE_NAME);

	return model_index != NONE && tag_loaded(BIPED_DEFINITION_TAG, ELITE_NAME) != NONE &&
		graph->animations.count > 0 &&
		TAG_BLOCK_GET_ELEMENT(&graph->animations, 0, struct animation)->node_list_checksum ==
			model_definition_get(model_index)->node_list_checksum;
}

/* adds the graph to the tag table; FALSE if there is no memory for it */
static boolean graph_add_to_tags(
	byte *graph)
{
	long existing, index, salt = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&existing);
	struct cache_file_tag_instance *grown, *instance;
	char *name;

	if (!instances)
		return FALSE;
	grown = malloc((existing + 1) * sizeof(*grown) + sizeof(ELITE_GRAPH_NAME));
	if (!grown)
		return FALSE;
	memcpy(grown, instances, existing * sizeof(*grown));
	for (index = 0; index < existing; index++)
		salt = MAX(salt, (long)((unsigned long)instances[index].tag_index >> 16));
	name = (char *)(grown + existing + 1);
	strcpy(name, ELITE_GRAPH_NAME);
	instance = &grown[existing];
	memset(instance, 0, sizeof(*instance));
	instance->group_tag = ANIMATION_GRAPH_TAG;
	instance->parent_group_tags[0] = NONE;
	instance->parent_group_tags[1] = NONE;
	instance->tag_index = ((salt + 1) << 16) | existing;
	instance->name = XBOX_ADDRESS(name);
	instance->base_address = XBOX_ADDRESS(graph);

	player_animations.original_instances = instances;
	player_animations.original_count = existing;
	player_animations.instances = grown;
	player_animations.tag_index = instance->tag_index;
	cache_files_set_tag_instances(grown, existing + 1);
	return TRUE;
}

/* ---------- public code */

/* cache_files.c: a map's tags have loaded (before the menus add theirs) */
void player_animations_loaded(
	void)
{
	struct animation_file_embedded const *file = graph_file_find();
	long elite_definition_index = tag_loaded(BIPED_DEFINITION_TAG, ELITE_NAME);
	byte *graph;

	player_animations.tag_index = NONE;
	player_animations.elite_definition_index = NONE;
	if (!file || elite_definition_index == NONE)
		return;
	graph = graph_build(file);
	if (!graph)
	{
		error(_error_silent, "player animations: %s is not a graph this build reads", ELITE_GRAPH_FILE);
		return;
	}
	if (!graph_fits_elite((struct animation_graph *)graph) || !graph_add_to_tags(graph))
	{
		error(_error_silent, "player animations: the Elite graph doesn't fit this map's Elite, or there is no room");
		free(graph);
		return;
	}
	player_animations.graph = graph;
	player_animations.elite_definition_index = elite_definition_index;
}

/* cache_files.c: the map's tags are about to go (after the menus' have) */
void player_animations_unloaded(
	void)
{
	/* (the game's free takes no NULL) */
	if (player_animations.instances)
	{
		cache_files_set_tag_instances(player_animations.original_instances, player_animations.original_count);
		free(player_animations.instances);
	}
	if (player_animations.graph)
		free(player_animations.graph);
	memset(&player_animations, 0, sizeof(player_animations));
	player_animations.tag_index = NONE;
	player_animations.elite_definition_index = NONE;
}

/* units.c: the Elite graph's tag index for a unit of this definition (the
map's Elite), or NONE */
long player_animations_graph_for(
	long definition_index)
{
	return definition_index == player_animations.elite_definition_index ? player_animations.tag_index : NONE;
}
