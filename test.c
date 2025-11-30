#include "lexer.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *token_type_str[] = {
	[TOKEN_ERROR]        = "TOKEN_ERROR",
	[TOKEN_END]          = "TOKEN_END",

	[TOKEN_PLUS]         = "TOKEN_PLUS",
	[TOKEN_PLUS_PLUS]    = "TOKEN_PLUS_PLUS",
	[TOKEN_MINUS]        = "TOKEN_MINUS",
	[TOKEN_MINUS_MINUS]  = "TOKEN_MINUS_MINUS",
	[TOKEN_SLASH]        = "TOKEN_SLASH",
	[TOKEN_PERC]         = "TOKEN_PERC",
	[TOKEN_STAR]         = "TOKEN_STAR",
	[TOKEN_AND]          = "TOKEN_AND",
	[TOKEN_HAT]          = "TOKEN_HAT",
	[TOKEN_PIPE]         = "TOKEN_PIPE",
	[TOKEN_EQ]           = "TOKEN_EQ",
	[TOKEN_ARROW]	     = "TOKEN_ARROW",
	[TOKEN_LSHIFT]       = "TOKEN_LSHIFT",
	[TOKEN_RSHIFT]       = "TOKEN_RSHIFT",
	[TOKEN_DOUBLE_EQ]    = "TOKEN_DOUBLE_EQ",
	[TOKEN_LESS_THAN]    = "TOKEN_LESS_THAN",
	[TOKEN_GREATER_THAN] = "TOKEN_GREATER_THAN",
	[TOKEN_LESS_EQ]      = "TOKEN_LESS_EQ",
	[TOKEN_GREATER_EQ]   = "TOKEN_GREATER_EQ",
	[TOKEN_NOT_EQ]       = "TOKEN_NOT_EQ",
	[TOKEN_PLUS_EQ]      = "TOKEN_PLUS_EQ",
	[TOKEN_MINUS_EQ]     = "TOKEN_MINUS_EQ",
	[TOKEN_STAR_EQ]      = "TOKEN_STAR_EQ",
	[TOKEN_SLASH_EQ]     = "TOKEN_SLASH_EQ",
	[TOKEN_AND_EQ]       = "TOKEN_AND_EQ",
	[TOKEN_HAT_EQ]       = "TOKEN_HAT_EQ",
	[TOKEN_PIPE_EQ]      = "TOKEN_PIPE_EQ",
	[TOKEN_PERC_EQ]      = "TOKEN_PERC_EQ",
	[TOKEN_LSHIFT_EQ]    = "TOKEN_LSHIFT_EQ",
	[TOKEN_RSHIFT_EQ]    = "TOKEN_RSHIFT_EQ",
	[TOKEN_OR]           = "TOKEN_OR",
	[TOKEN_DOUBLE_AND]   = "TOKEN_DOUBLE_AND",
	[TOKEN_COLON]        = "TOKEN_COLON",
	[TOKEN_SEMICOLON]    = "TOKEN_SEMICOLON",
	[TOKEN_DOT]          = "TOKEN_DOT",
	[TOKEN_BANG]         = "TOKEN_BANG",
	[TOKEN_COMMA]        = "TOKEN_COMMA",
	[TOKEN_LPAREN]       = "TOKEN_LPAREN",
	[TOKEN_RPAREN]       = "TOKEN_RPAREN",
	[TOKEN_LSQUARE]      = "TOKEN_LSQUARE",
	[TOKEN_RSQUARE]      = "TOKEN_RSQUARE",
	[TOKEN_LCURLY]       = "TOKEN_LCURLY",
	[TOKEN_RCURLY]       = "TOKEN_RCURLY",

	[TOKEN_INTEGER]      = "TOKEN_INTEGER",
	[TOKEN_FLOAT]        = "TOKEN_FLOAT",
	[TOKEN_IDENTIFIER]   = "TOKEN_IDENTIFIER",
	[TOKEN_STRING]       = "TOKEN_STRING",
	[TOKEN_CHAR]         = "TOKEN_CHAR",

	[TOKEN_WHILE]        = "TOKEN_WHILE",
	[TOKEN_FOR]          = "TOKEN_FOR",
	[TOKEN_GOTO]         = "TOKEN_GOTO",
	[TOKEN_IF]           = "TOKEN_IF",
	[TOKEN_ELSE]         = "TOKEN_ELSE",
	[TOKEN_SWITCH]       = "TOKEN_SWITCH",
	[TOKEN_CASE]         = "TOKEN_CASE",
	[TOKEN_DO]           = "TOKEN_DO",
	[TOKEN_DEFER]        = "TOKEN_DEFER",
	[TOKEN_MODULE]       = "TOKEN_MODULE",

	[TOKEN_STATIC]       = "TOKEN_STATIC",
	[TOKEN_CONST]        = "TOKEN_CONST",
	[TOKEN_EXTERN]       = "TOKEN_EXTERN",
	[TOKEN_VOLATILE]     = "TOKEN_VOLATILE",
};

trie_node *keywords;

void lexer_print_token(token *t)
{
	printf("%s: ", token_type_str[t->type]);
	for (usize i=0; i < t->lexeme_len; i++) {
		printf("%c", t->lexeme[i]);
	}
}

static void add_token(lexer *l, token_type type, usize len)
{
	token *t = arena_alloc(l->allocator, sizeof(token));
	t->type = type;
	t->lexeme_len = len;
	t->lexeme = l->source + l->index;
	t->position.row = l->row;
	t->position.column = l->column;

	if (!l->tokens) {
		l->tokens = t;
		l->tail = t;
	} else {
		l->tail->next = t;
		l->tail = t;
	}
}

static void add_error(lexer *l, char *msg)
{
	token *t = arena_alloc(l->allocator, sizeof(token));
	t->type = TOKEN_ERROR;
	t->lexeme_len = strlen(msg);
	t->lexeme = msg;
	t->position.row = l->row;
	t->position.column = l->column;

	if (!l->tokens) {
		l->tokens = t;
		l->tail = t;
	} else {
		l->tail->next = t;
		l->tail = t;
	}
}

static void parse_number(lexer *l)
{
	char c = l->source[l->index];
	/* Is the number a float? */
	bool f = false;
	usize len = 0;

	while (isdigit(c)) {
		/* If a dot is found, and the character after it is a digit, this is a float. */
		if (l->source[l->index+1] == '.' && isdigit(l->source[l->index+2])) {
			f = true;
			len += 3;
			l->index += 3;
		} else {
			len += 1;
			l->index += 1;
		}
		c = l->source[l->index];
	}
	l->index -= len;
	if (f) {
		add_token(l, TOKEN_FLOAT, len);
	} else {
		add_token(l, TOKEN_INTEGER, len);
	}
	l->index += len;
}

static void parse_identifier(lexer *l)
{
	char c = l->source[l->index];
	usize len = 0;

	while (isalnum(c) || c == '_') {
		len += 1;
		l->index += 1;
		c = l->source[l->index];
	}
	l->index -= len;
	token_type keyword = trie_get(keywords, l->source + l->index, len);
	if (keyword) {
		add_token(l, keyword, len);
	} else {
		add_token(l, TOKEN_IDENTIFIER, len);
	}
	l->index += len;
}

static void parse_string(lexer *l)
{
	char c = l->source[l->index];
	usize len = 0;

	while (c != '"') {
		if (c == '\0' || c == '\n') {
			printf("%c", c);
			l->index -= len;
			add_error(l, "unclosed string literal.");
			l->index += len;
			return;
		}
		len += 1;
		l->index += 1;
		c = l->source[l->index];
	}
	l->index -= len;
	add_token(l, TOKEN_STRING, len);
	l->index += len + 1;
}

static bool parse_special(lexer *l)
{
	switch (l->source[l->index]) {
	case '+':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_PLUS_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '+') {
			add_token(l, TOKEN_PLUS_PLUS, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_PLUS, 1);
			l->index += 1;
		}
		return true;
	case '-':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_MINUS_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '-') {
			add_token(l, TOKEN_MINUS_MINUS, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '>') {
			add_token(l, TOKEN_ARROW, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_MINUS, 1);
			l->index += 1;
		}
		return true;
	case '/':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_SLASH_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_SLASH, 1);
			l->index += 1;
		}
		return true;
	case '*':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_STAR_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_STAR, 1);
			l->index += 1;
		}
		return true;
	case '%':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_PERC_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_PERC, 1);
			l->index += 1;
		}
		return true;
	case '&':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_AND_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '&') {
			add_token(l, TOKEN_DOUBLE_AND, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_AND, 1);
			l->index += 1;
		}
		return true;
	case '^':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_HAT_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_HAT, 1);
			l->index += 1;
		}
		return true;
	case '|':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_PIPE_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '|') {
			add_token(l, TOKEN_OR, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_PIPE, 1);
			l->index += 1;
		}
		return true;
	case '=':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_DOUBLE_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_EQ, 1);
			l->index += 1;
		}
		return true;
	case '>':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_GREATER_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '>') {
			if (l->source[l->index+2] == '=') {
				add_token(l, TOKEN_RSHIFT_EQ, 3);
				l->index += 3;
				return true;
			}
			add_token(l, TOKEN_RSHIFT, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_GREATER_THAN, 1);
			l->index += 1;
		}
		return true;
	case '<':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_LESS_EQ, 2);
			l->index += 2;
		} else if (l->source[l->index+1] == '<') {
			if (l->source[l->index+2] == '=') {
				add_token(l, TOKEN_LSHIFT_EQ, 3);
				l->index += 3;
				return true;
			}
			add_token(l, TOKEN_LSHIFT, 2);
			l->index += 2;
		} else {
			add_token(l, TOKEN_LESS_THAN, 1);
			l->index += 1;
		}
		return true;
	case '!':
		if (l->source[l->index+1] == '=') {
			add_token(l, TOKEN_NOT_EQ, 2);
			l->index += 2;
		}  else {
			add_token(l, TOKEN_BANG, 1);
			l->index += 1;
		}
		return true;
	case ':':
		add_token(l, TOKEN_COLON, 1);
		l->index += 1;
		return true;
	case ';':
		add_token(l, TOKEN_SEMICOLON, 1);
		l->index += 1;
		return true;
	case '.':
		add_token(l, TOKEN_DOT, 1);
		l->index += 1;
		return true;
	case ',':
		add_token(l, TOKEN_COMMA, 1);
		l->index += 1;
		return true;
	case '(':
		add_token(l, TOKEN_LPAREN, 1);
		l->index += 1;
		return true;
	case ')':
		add_token(l, TOKEN_RPAREN, 1);
		l->index += 1;
		return true;
	case '[':
		add_token(l, TOKEN_LSQUARE, 1);
		l->index += 1;
		return true;
	case ']':
		add_token(l, TOKEN_RSQUARE, 1);
		l->index += 1;
		return true;
	case '{':
		add_token(l, TOKEN_LCURLY, 1);
		l->index += 1;
		return true;
	case '}':
		add_token(l, TOKEN_RCURLY, 1);
		l->index += 1;
		return true;
	case '\'':
		if (l->source[l->index+1] == '\\') {
			if (l->source[l->index+3] != '\'') {
				add_error(l, "unclosed character literal.");
				return true;
			}
			l->index += 1;
			add_token(l, TOKEN_CHAR, 2);
			l->index += 3;
			return true;
		} else {
			if (l->source[l->index+2] != '\'') {
				add_error(l, "unclosed character literal.");
				return false;
			}
			l->index += 1;
			add_token(l, TOKEN_CHAR, 1);
			l->index += 2;
			return true;
		}
	default:
		return false;
	}
}

static void parse(lexer *l)
{
	char c;

	while (l->index <= l->size) {
		c = l->source[l->index];
		l->column += 1;

		if (c == '\n') {
			l->index += 1;
			l->row += 1;
			l->column = 0;
			continue;
		}

		if (isspace(c)) {
			l->index += 1;
			continue;
		}

		usize head = l->index;

		if (parse_special(l)) {
			l->column += (l->index - head - 1);
			continue;
		}

		if (isdigit(c)) {
			parse_number(l);
			l->column += (l->index - head - 1);
			continue;
		}

		if (isalpha(c)) {
			parse_identifier(l);
			l->column += (l->index - head - 1);
			continue;
		}

		if (c == '"') {
			l->index += 1;
			parse_string(l);
			l->column += (l->index - head - 1);
			continue;
		}

		l->index += 1;
	}
}

lexer *lexer_init(char *source, usize size, arena *arena)
{
	lexer *lex = arena_alloc(arena, sizeof(lexer));
	lex->column = 0;
	lex->row = 0;
	lex->index = 0;
	lex->size = size;
	lex->tokens = 0;
	lex->tail = 0;
	lex->allocator = arena;
	lex->source = source;

	keywords = arena_alloc(arena, sizeof(trie_node));
	trie_insert(keywords, lex->allocator, "while", TOKEN_WHILE);
	trie_insert(keywords, lex->allocator, "for", TOKEN_FOR);
	trie_insert(keywords, lex->allocator, "goto", TOKEN_GOTO);
	trie_insert(keywords, lex->allocator, "if", TOKEN_IF);
	trie_insert(keywords, lex->allocator, "else", TOKEN_ELSE);
	trie_insert(keywords, lex->allocator, "switch", TOKEN_SWITCH);
	trie_insert(keywords, lex->allocator, "case", TOKEN_CASE);
	trie_insert(keywords, lex->allocator, "do", TOKEN_DO);
	trie_insert(keywords, lex->allocator, "defer", TOKEN_DEFER);
	trie_insert(keywords, lex->allocator, "module", TOKEN_MODULE);
	trie_insert(keywords, lex->allocator, "static", TOKEN_STATIC);
	trie_insert(keywords, lex->allocator, "const", TOKEN_CONST);
	trie_insert(keywords, lex->allocator, "extern", TOKEN_EXTERN);
	trie_insert(keywords, lex->allocator, "volatile", TOKEN_VOLATILE);

	parse(lex);

	return lex;
}
