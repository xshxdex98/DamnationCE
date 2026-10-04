/*
TAG_GROUPS.C
*/

/* ---------- headers */

#include "cseries.h"
#include "tag_files.h"
#include "byte_swapping.h"
#include "errors.h"
#include "tag_groups.h"

/* port: port/linux/game/custom_edition_cache.c and cache_files.c */
boolean custom_edition_cache_tags_loaded(void);
boolean tag_index_is_group(long tag_index, long group_tag);

/* port: Halo PC's release build reads a Custom Edition map's tags unchecked,
and maps made for it can hold an index past a block's end or an offset past
a tag data's, which never stopped a game there (docs/custom_edition_caches.md).
With such a map loaded, one out of range gets a zeroed spare of the size
asked for, and the first few are logged, instead of the game stopping. Xbox
maps keep their checks. */
#define CUSTOM_EDITION_SPARE_BYTES 0x10000
#define MAXIMUM_CUSTOM_EDITION_RANGE_MESSAGES 8

static void *custom_edition_out_of_range(
	char const *what,
	long index,
	long count,
	long size)
{
	static long spare[CUSTOM_EDITION_SPARE_BYTES / sizeof(long)];
	static long messages;

	if (size < 0 || size > (long)sizeof(spare))
		return NULL;
	if (messages++ < MAXIMUM_CUSTOM_EDITION_RANGE_MESSAGES)
	{
		error(_error_silent, "custom edition: #%ld is not a valid %s index in [#0,#%ld): given zeros", index, what,
			count);
	}
	/* (zeroed each time: the reader may have written to it) */
	csmemset(spare, 0, (size_t)size);
	return spare;
}

/* ---------- public code */

long verify_tag_reference(
	const struct tag_reference *reference)
{
	long index;

	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3055, reference);
	/* port: a protected Custom Edition map has its tag names replaced and
	its references' names emptied, so a reference is taken by its index */
	if (custom_edition_cache_tags_loaded())
		return tag_index_is_group(reference->index, reference->group_tag) ? reference->index : NONE;
#ifdef HALO_64BIT
	index = tag_loaded(reference->group_tag, TAG_REFERENCE_NAME(reference));
#else
	index = tag_loaded(reference->group_tag, reference->name);
#endif
	
	match_vassert(
		"c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3061, reference->index==index,
		csprintf(temporary,
			"tag reference \"%s\" and actual index do not match: is %08lX but should be %08lX",
#ifdef HALO_64BIT
			TAG_REFERENCE_NAME(reference),
#else
			reference->name,
#endif
			reference->index,
			index));

	return index;
}

void* tag_data_get_pointer(
	const struct tag_data *data,
	long offset, 
	long size) 
{
	if (custom_edition_cache_tags_loaded() && (size < 0 || offset < 0 || offset + size > data->size))
	{
		void *spare = custom_edition_out_of_range("tag data byte", offset + size, data->size, size);

		if (spare)
			return spare;
	}
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3073, size>=0);
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3074, offset>=0 && offset+size<=data->size);

#ifdef HALO_64BIT
	return (void *)((byte *)TAG_DATA_ADDRESS(data) + offset);
#else
	return (void *)((byte *)data->address + offset);
#endif
}

void *tag_block_get_element_with_size(
	const struct tag_block *block,
	long index, 
	long element_size) 
{
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3084, block);
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3085, block->count>=0);
#ifndef HALO_64BIT
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3086, !block->definition || block->definition->element_size==element_size);
#endif

	if (custom_edition_cache_tags_loaded() && (index < 0 || index >= block->count || !block->address))
	{
#ifdef HALO_64BIT
		void *spare = custom_edition_out_of_range("<unknown>", index, block->count, element_size);
#else
		void *spare = custom_edition_out_of_range(block->definition ? block->definition->name : "<unknown>", index,
			block->count, element_size);
#endif

		if (spare)
			return spare;
	}
	match_vassert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3089, index>=0 && index<block->count,
		csprintf(temporary,
			"#%d is not a valid %s index in [#0,#%d)",
			index,
#ifdef HALO_64BIT
			"<unknown>", block->count));
#else
			block->definition ? block->definition->name : "<unknown>", block->count));
#endif
	match_assert("c:\\halo\\SOURCE\\tag_files\\tag_groups.c", 3090, block->address);

#ifdef HALO_64BIT
	return (void *)((byte *)TAG_BLOCK_ADDRESS(block) + (index * element_size));
#else
	return (void *)((byte *)block->address + (index * element_size));
#endif
}
