#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "utils.h"
#include <stdbool.h>

struct _ast_node;

typedef enum {
	OP_PLUS, // +
	OP_MINUS, // -
	OP_DIV, // /
	OP_MUL, // *
	OP_EQ, // ==
	OP_ASSIGN, // =
	OP_AND, // &&
	OP_OR, // ||
	OP_NEQ, // !=
	OP_GT, // >
	OP_LT, // <
	OP_GE, // >=
	OP_LE, // <=
	OP_RSHIFT_EQ, // >>=
	OP_LSHIFT_EQ, // <<=
	OP_BOR, // |
	OP_BAND, // &
	OP_BXOR, // ^
	OP_MOD, // %
	OP_PLUS_EQ, // +=
	OP_MINUS_EQ, // -=
	OP_DIV_EQ, // /=
	OP_MUL_EQ, // *=
	OP_BOR_EQ, // |=
	OP_BAND_EQ, // &=
	OP_BXOR_EQ, // ^=
	OP_MOD_EQ, // %=
} binary_op;

typedef enum {
	UOP_INCR, // ++
	UOP_MINUS, // -
	UOP_DECR, // --
	UOP_DEREF, // *
	UOP_REF, // &
	UOP_NOT, // !
} unary_op;

typedef enum {
	LAYOUT_AUTO,
	LAYOUT_PACKED,
	LAYOUT_EXTERN
} struct_layout;

typedef struct _member {
	struct _ast_node *type;
	char *name;
	usize name_len;
	struct _member *next;
	usize offset;
} member;

typedef struct {
	char *name;
	usize name_len;
	member *params;
} function;

typedef struct _variant {
	struct _ast_node *value;
	char *name;
	usize name_len;
	struct _variant *next;
} variant;

typedef enum {
	NODE_IDENTIFIER,
	NODE_INTEGER,
	NODE_FLOAT,
	NODE_STRING,
	NODE_CHAR,
	NODE_CAST,
	NODE_UNARY,
	NODE_BINARY,
	NODE_RANGE,
	NODE_ARRAY_SUBSCRIPT,
	NODE_ACCESS,
	NODE_CALL,
	NODE_POSTFIX,
	NODE_BREAK,
	NODE_RETURN,
	NODE_LABEL,
	NODE_GOTO,
	NODE_IMPORT,
	NODE_FOR,
	NODE_WHILE,
	NODE_IF,
	NODE_COMPOUND,
	NODE_ENUM,
	NODE_STRUCT,
	NODE_UNION,
	NODE_VAR_DECL,
	NODE_FUNCTION,
	NODE_PTR_TYPE,
	NODE_TERNARY, /* TODO */
	NODE_SWITCH, /* TODO */
	NODE_STRUCT_INIT,
	NODE_UNIT,
} node_type;

#define PTR_SLICE 0x0
#define PTR_RAW 0x1

#define LOOP_WHILE 0x1
#define LOOP_UNTIL 0x2
#define LOOP_AFTER 0x4

typedef struct _ast_node {
	node_type type;
	source_pos position;
	union {
		struct {
			struct _ast_node *type;
			u8 flags;
		} ptr_type;
		struct {
			char *name;
			usize name_len;
		} label; // both label and goto
		struct {
			struct _ast_node *left;
			struct _ast_node *right;
			binary_op operator;
		} binary;
		struct {
			struct _ast_node *right;
			unary_op operator;
		} unary;
		i64 integer;
		f64 flt; // float
		struct {
			char *start;
			usize len;
		} string;
		char ch; // char;
		struct {
			struct _ast_node *condition;
			struct _ast_node *then;
			struct _ast_node *otherwise;
		} ternary;
		struct {
			struct _ast_node *value;
			struct _ast_node *type;
		} cast;
		struct {
			struct _ast_node *expr;
			struct _ast_node *index;
		} subscript;
		struct {
			struct _ast_node *expr;
			struct _ast_node *member;
		} access;
		struct {
			struct _ast_node *expr;
			struct _ast_node *next;
		} unit_node;
		struct {
			/* This should be a list of unit_node */
			struct _ast_node *parameters;
			usize param_len;
			char *name;
			usize name_len;
		} call;
		struct {
			struct _ast_node *value;
		} ret;
		struct {
			/* This should be an access. */
			struct _ast_node *path;
		} import;
		struct {
			/* These should be lists of unit_node */
			struct _ast_node *slices;
			struct _ast_node *captures;
			int capture_len;
			int slice_len;
			struct _ast_node* body;
		} fr; // for
		struct {
			struct _ast_node *condition;
			struct _ast_node *body;
			u8 flags;
		} whle; // while
		struct {
			struct _ast_node **statements;
			usize stmt_len;
		} compound;
		struct {
			struct _ast_node *value;
			char *name;
			usize name_len;
			struct _ast_node *type;
		} var_decl;
		struct {
			member *members;
			char *name;
			usize name_len;
		} structure;
		struct {
			member *parameters;
			usize parameters_len;
			char *name;
			usize name_len;
			struct _ast_node *type;
			struct _ast_node *body;
		} function;
		struct {
			variant *variants;
			char *name;
			usize name_len;
		} enm; // enum
		struct {
			struct _ast_node *members;
			usize members_len;
		} struct_init;
	} expr;
} ast_node;

typedef struct {
	token *tokens;
	token *previous;
	ast_node *ast;
	arena *allocator;
} parser;

parser *parser_init(lexer *l, arena *allocator);

#endif
