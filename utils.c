#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

u64 parse_int(char *s, usize len)
{
	u64 int_part = 0;
	for (usize i=0; i < len; i++) {
		int_part = (int_part * 10) + (s[i] - '0');
	}

	return int_part;
}

f64 parse_float(char *s, usize len)
{
	f64 decimal_part = (f64)parse_int(s, len);
	usize point_pos = 0;

	for (usize i=0; i < len; i++) {
		if (s[i] == '.') {
			point_pos = i;
			break;
		}
	}
	point_pos += 1;

	for (usize i=0; i < len - point_pos; i++) {
		decimal_part /= 10.0;
	}

	return decimal_part;
}


void trie_insert(trie_node *root, arena *a, char *key, uint16_t value)
{
	trie_node *node = root;
	while (*key) {
		if (!node->children[(usize)*key]) {
			node->children[(usize)*key] = arena_alloc(a, sizeof(trie_node));
			memset(node->children[(usize)*key], 0x0, sizeof(trie_node));
		}
		node = node->children[(usize)*key];

		key++;
	}

	node->value = value;
}

uint16_t trie_get(trie_node *root, char *key, usize len)
{
	trie_node *node = root;
	for (usize i=0; i < len; i++) {
		if (!node->children[(usize)(key[i])]) {
			return 0;
		}
		node = node->children[(usize)(key[i])];
	}

	return node->value;
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

static usize align_forward(usize ptr, usize align) {
	uintptr_t p = ptr;
	uintptr_t a = (uintptr_t)align;
	uintptr_t modulo = p & (a - 1);

	if (modulo != 0) {
		p += a - modulo;
	}
	return (usize)p;
}

arena arena_init(usize size)
{
	return (arena){
		.capacity = size,
		.position = 0,
		.memory = malloc(size),
	};
}

void *arena_alloc(arena *a, usize size) {
	uintptr_t current_addr = (uintptr_t)a->memory + a->position;
	uintptr_t padding = align_forward(current_addr, DEFAULT_ALIGNMENT) - current_addr;
	if (a->position + padding + size > a->capacity) return NULL;
	void *ret = (unsigned char *)a->memory + a->position + padding;
	a->position += (size + padding); 

	return ret;
}

snapshot arena_snapshot(arena *a)
{
	return a->position;
}

void arena_reset_to_snapshot(arena *a, snapshot s)
{
	a->position = s;
}

void arena_reset(arena *a)
{
	arena_reset_to_snapshot(a, 0);
}

void arena_deinit(arena a)
{
	free(a.memory);
}
