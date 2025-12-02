#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "utils.h"

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

typedef struct {
	char *type_name;
	usize type_len;
	char *name;
	usize name_len;
} member;

typedef struct {
	char *name;
	usize name_len;
	member *params;
} function;

typedef enum {
	NODE_IDENTIFIER,
	NODE_INTEGER,
	NODE_FLOAT,
	NODE_STRING,
	NODE_CHAR,
	NODE_CAST,
	NODE_UNARY,
	NODE_BINARY,
	NODE_EQUAL,
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
	NODE_TERNARY,
	NODE_SWITCH,
	NODE_UNIT,
} node_type;

typedef struct _ast_node {
	node_type type;
	union {
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
		u64 integer;
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
			char *type;
			usize type_len;
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
		} whle; // while
		struct {
			struct _ast_node **statements;
			usize stmt_len;
		} compound;
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
