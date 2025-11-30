#ifndef SEMA_H
#define SEMA_H

typedef enum {
	TYPE_VOID,
	TYPE_PTR,
	TYPE_I8,
	TYPE_I16,
	TYPE_I32,
	TYPE_I64,
	TYPE_U8,
	TYPE_U16,
	TYPE_U32,
	TYPE_U64,
	TYPE_STRUCT,
	TYPE_UNION,
	TYPE_ENUM,
} type_tag;

typedef struct _type {
	type_tag tag;
	union {
		u8 integer;
		u8 flt; // float
		struct {
			bool is_const;
			bool is_volatile;
			u16 alignment;
			struct _type child;
		} ptr;
		struct {
			usize len;
			struct _type child;
		} array;
		struct {
			struct_layout layout;
			char *name;
			usize name_len;
			usize alignment;
			member *members;
			function_decl *decls;
		} structure;
		struct {
			struct_layout layout;
			char *name;
			usize name_len;
			usize alignment;
			member *members;
			function_decl *decls;
		} enum;
	} data;
} type;

#endif
