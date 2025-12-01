#ifndef LEXER_H
#define LEXER_H

#include "utils.h"

typedef enum {
	TOKEN_ERROR,
	TOKEN_END,

	TOKEN_PLUS, // +
	TOKEN_PLUS_PLUS, // ++
	TOKEN_MINUS, // -
	TOKEN_MINUS_MINUS, // --
	TOKEN_SLASH, // /
	TOKEN_PERC, // %
	TOKEN_STAR, // *
	TOKEN_AND, // &
	TOKEN_HAT, // ^
	TOKEN_PIPE, // |
	TOKEN_EQ, // =
	TOKEN_ARROW, // ->
	TOKEN_LSHIFT, // <<
	TOKEN_RSHIFT, // >>
	TOKEN_DOUBLE_EQ, // ==
	TOKEN_LESS_THAN, // <
	TOKEN_GREATER_THAN, // >
	TOKEN_LESS_EQ, // <=
	TOKEN_GREATER_EQ, // >=
	TOKEN_NOT_EQ, // !=
	TOKEN_PLUS_EQ, // +=
	TOKEN_MINUS_EQ, // -=
	TOKEN_STAR_EQ, // *=
	TOKEN_SLASH_EQ, // /=
	TOKEN_AND_EQ, // &=
	TOKEN_HAT_EQ, // ^=
	TOKEN_PIPE_EQ, // |=
	TOKEN_PERC_EQ, // %=
	TOKEN_LSHIFT_EQ, // <<=
	TOKEN_RSHIFT_EQ, // >>=
	TOKEN_OR, // ||
	TOKEN_DOUBLE_AND, // &&
	TOKEN_COLON, // :
	TOKEN_SEMICOLON, // ;
	TOKEN_DOT, // .
	TOKEN_BANG, // !
	TOKEN_COMMA, // ,
	TOKEN_LPAREN, // (
	TOKEN_RPAREN, // )
	TOKEN_LSQUARE, // [
	TOKEN_RSQUARE, // ]
	TOKEN_LCURLY, // {
	TOKEN_RCURLY, // }

	TOKEN_INTEGER,
	TOKEN_FLOAT,
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_CHAR,

	TOKEN_GOTO,
	TOKEN_LOOP,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_SWITCH,
	TOKEN_BREAK,
	TOKEN_DO,
	TOKEN_DEFER,
	TOKEN_MODULE,
	TOKEN_RETURN,

	TOKEN_STATIC,
	TOKEN_CONST,
	TOKEN_EXTERN,
	TOKEN_VOLATILE,

	TOKEN_STRUCT,
	TOKEN_ENUM,
	TOKEN_UNION
} token_type;

typedef struct _token {
	token_type type;
	source_pos position;
	char *lexeme;
	usize lexeme_len;
	struct _token *next;
} token;

typedef struct {
	usize column, row, index, size;
	char *source;
	token *tokens;
	token *tail;
	arena *allocator;
} lexer;

lexer *lexer_init(char *source, usize size, arena *arena);

#endif
