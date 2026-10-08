/*
STACK_MEMORY_POOL.H
*/

#ifndef __STACK_MEMORY_POOL_H
#define __STACK_MEMORY_POOL_H
#pragma once

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct stack_memory_pool
{
	char const *name;
	byte *base_address;
	long size;
	long maximum_block_count;
	long next_block_index;
	long bytes_used;
	long maximum_bytes_used;
	unsigned long block_count;
	unsigned long maximum_block_count_used;
	long largest_block_size;
	boolean disable_compaction;
	byte unused29[3];
	struct stack_memory_pool_block *first_block;
	struct stack_memory_pool_block *last_block;
	struct stack_memory_pool_block *blocks[1];
};

struct stack_memory_pool;

/* ---------- prototypes/STACK_MEMORY_POOL.C */

void stack_memory_pool_reset(
	struct stack_memory_pool *pool);
void *pool_new_pointer(
	struct stack_memory_pool *pool,
	long allocation_size,
	char const *file,
	unsigned long line);
void dispose_pointer(
	struct stack_memory_pool *pool,
	void *pointer);
void *pool_resize_pointer(
	struct stack_memory_pool *pool,
	void *pointer,
	long allocation_size,
	char const *file,
	unsigned long line);

/* ---------- globals */

/* ---------- public code */

#endif // __STACK_MEMORY_POOL_H
