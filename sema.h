#ifndef SEMA_H
#define SEMA_H

#include "parser.h"
#include "stb_ds.h"
#include "utils.h"

typedef enum {
	TYPE_VOID,
	TYPE_PTR,
	TYPE_FLOAT,
	TYPE_INTEGER,
	TYPE_UINTEGER,
	TYPE_STRUCT,
	TYPE_UNION,
	TYPE_ENUM,
	TYPE_GENERIC,
} type_tag;

typedef struct _type {
	type_tag tag;
	usize size;
	usize alignment;
	union {
		u8 integer;
		u8 flt; // float
		struct {
			bool is_const;
			bool is_volatile;
			u16 alignment;
			struct _type *child;
		} ptr;
		struct {
			usize len;
			struct _type *child;
		} array;
		struct {
			char *name;
			usize name_len;
			member *members;
		} structure;
		struct {
			char *name;
			usize name_len;
			variant *variants;
		} enm;
	} data;
} type;

typedef struct {
	arena *allocator;
	ast_node *ast;
} sema;

sema *sema_init(parser *p, arena *a);

#endif
