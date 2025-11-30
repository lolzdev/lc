#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t usize;

typedef float f32;
typedef double f64;

typedef struct {
	usize capacity;
	usize position;
	void* memory;
} arena;

typedef usize snapshot;

/*
 * NOTE(ernesto): faulty initialization is signalided by the arena.memory
 * being null. It is the responsability of the caller to check for fulty
 * initialization.
 */
arena arena_init(usize size);
/*
 * Returns null on unsuccessfull allocation.
 * In this implemention an allocation is only unsuccessfull if the arena
 * does not have enough memory to allocate the requested space
 */
void *arena_alloc(arena *a, usize size);
snapshot arena_snapshot(arena a);
void arena_reset_to_snapshot(arena *a, snapshot s);
void arena_reset(arena *a);
/* This call should never fail, also, do we even care if it does? */
void arena_deinit(arena a);

typedef struct _trie_node {
	uint16_t value;
	struct _trie_node *children[256];
} trie_node;

void trie_insert(trie_node *root, arena *a, char *key, uint16_t value);
uint16_t trie_get(trie_node *root, char *key, usize len);

typedef struct {
	usize row, column;
} source_pos;

#endif
