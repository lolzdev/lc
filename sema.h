#ifndef SEMA_H
#define SEMA_H

#include <stdbool.h>
#include "parser.h"
#include "stb_ds.h"
#include "utils.h"

typedef enum {
	TYPE_VOID,
	TYPE_BOOL,
	TYPE_PTR,
	TYPE_SLICE,
	TYPE_FLOAT,
	TYPE_INTEGER,
	TYPE_UINTEGER,
	TYPE_STRUCT,
	TYPE_UNION,
	TYPE_ENUM, /* TODO */
	TYPE_GENERIC, /* TODO */
} type_tag;

typedef struct _type {
	type_tag tag;
	usize size;
	usize alignment;
	char *name;
	union {
		u8 integer;
		u8 flt; // float
		struct {
			bool is_const;
			bool is_volatile;
			struct _type *child;
		} ptr;
		struct {
			usize len;
			bool is_const;
			bool is_volatile;
			struct _type *child;
		} slice;
		struct {
			char *name;
			usize name_len;
			member *members;
			struct { char *key; struct _type *value; } *member_types;
		} structure;
		struct {
			char *name;
			usize name_len;
			variant *variants;
		} enm; /* TODO */
	} data;
} type;

typedef struct {
	char *name;
	type *type;
	type **parameters;
} prototype;

typedef struct _scope {
	struct _scope *parent;
	struct { char *key; type *value; } *defs;
} scope;

typedef struct {
	arena *allocator;
	ast_node *ast;
} sema;

sema *sema_init(parser *p, arena *a);

#endif
