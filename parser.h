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
	usize offset;
} member;

typedef struct {
	char *name;
	usize name_len;
	member *params;
} function_decl;

typedef enum {
	NODE_IDENTIFIER,
	NODE_INTEGER,
	NODE_FLOAT,
	NODE_STRING,
	NODE_CHAR,
	NODE_CAST,
	NODE_ARRAY_SUBSCRIPT,
	NODE_ACCESS,
	NODE_CALL,
	NODE_POSTFIX,
	NODE_UNARY,
	NODE_BINARY,
	NODE_GOTO,
	NODE_BREAK,
	NODE_CASE,
	NODE_SWITCH,
	NODE_FOR,
	NODE_DO,
	NODE_WHILE,
	NODE_IF,
	NODE_RETURN,
	NODE_COMPOUND,
	NODE_TYPEDEF,
	NODE_ENUM,
	NODE_STRUCT,
	NODE_UNION,
	NODE_VAR_DECL,
	NODE_FUNCTION_DEF,
	NODE_FUNCTION_DECL,
	NODE_TERNARY,
	NODE_UNIT,
} node_type;

typedef struct _ast_node {
	node_type type;
	union {
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
			char *member;
			usize member_len;
		} access;
		struct {
			struct _ast_node *expr;
			struct _ast_node *next;
		} unit_node;
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
