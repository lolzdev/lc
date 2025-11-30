#include "parser.h"
#include <stdbool.h>
#include <stdio.h>

ast_node *parse_expression(parser *p);

static void advance(parser *p)
{
	p->previous = p->tokens;
	if (p->tokens)
		p->tokens = p->tokens->next;
}

static token *peek(parser *p)
{
	return p->tokens;
}

static bool match_peek(parser *p, token_type type)
{
	if (p->tokens) {
		return p->tokens->type == type;
	} else {
		return false;
	}
}

static bool match(parser *p, token_type type)
{
	if (p->tokens) {
		if (p->tokens->type == type) {
			advance(p);
			return true;
		}
	}
	return false;
}

static void parser_sync(parser *p)
{
	advance(p);

	while (p->tokens) {
		if (p->previous->type == TOKEN_SEMICOLON || p->previous->type == TOKEN_RCURLY) {
			return;
		}

		switch (p->tokens->type) {
			case TOKEN_STRUCT:
			case TOKEN_ENUM:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_FOR:
			case TOKEN_DO:
			case TOKEN_RETURN:
			case TOKEN_SWITCH:
				return;
			default:
				advance(p);
		}
	}
}

static void error(parser *p, char *msg)
{
	printf("\x1b[31m\x1b[1merror\x1b[0m\x1b[1m:%ld:%ld:\x1b[0m %s\n", p->previous->position.row, p->previous->position.column, msg);
	parser_sync(p);
}

static ast_node *parse_factor(parser *p)
{
	token *t = peek(p);
	if (match(p, TOKEN_INTEGER)) {
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_INTEGER;
		node->expr.integer = parse_int(t->lexeme, t->lexeme_len);
		return node;
	} else if (match(p, TOKEN_FLOAT)) {
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_FLOAT;
		node->expr.flt = parse_float(t->lexeme, t->lexeme_len);
		return node;
	} else if (match(p, TOKEN_IDENTIFIER)) {
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_IDENTIFIER;
		node->expr.string.start = t->lexeme;
		node->expr.string.len = t->lexeme_len;
		return node;
	} else if (match(p, TOKEN_STRING)) {
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_STRING;
		node->expr.string.start = t->lexeme;
		node->expr.string.len = t->lexeme_len;
		return node;
	} else if (match(p, TOKEN_CHAR)) {
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_CHAR;
		if (t->lexeme_len == 2) {
			char c;
			switch (t->lexeme[1]) {
				case 'n': c = '\n'; break;
				case 't': c = '\t'; break;
				case 'r': c = '\r'; break;
				case '0': c = '\0'; break;
				case '\\': c = '\\'; break;
				case '\'': c = '\''; break;
				default:
					error(p, "invalid escape code.");
					return NULL;
			}
			node->expr.ch = c;
		} else {
			node->expr.ch = *(t->lexeme);
		}
		return node;
	} else if (match(p, TOKEN_LPAREN)) {
		ast_node *node = parse_expression(p);
		if (!match(p, TOKEN_RPAREN)) {
			error(p, "unclosed parenthesis");
			return NULL;
		}

		return node;
	}

	return NULL;
}

ast_node *parse_unary(parser *p)
{
	if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS) || match(p, TOKEN_MINUS_MINUS) || match(p, TOKEN_STAR) || match(p, TOKEN_AND) || match(p, TOKEN_BANG)) {
		unary_op op;
		switch (p->previous->type) {
			case TOKEN_PLUS_PLUS:
				op = UOP_INCR;
				break;
			case TOKEN_MINUS:
				op = UOP_MINUS;
				break;
			case TOKEN_MINUS_MINUS:
				op = UOP_DECR;
				break;
			case TOKEN_STAR:
				op = UOP_DEREF;
				break;
			case TOKEN_AND:
				op = UOP_REF;
				break;
			case TOKEN_BANG:
				op = UOP_NOT;
				break;
			default:
				goto end;
		}

		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_UNARY;
		node->expr.unary.operator = op;
		node->expr.unary.right = parse_expression(p);

		return node;
	}

end:
	return parse_factor(p);
}

ast_node *parse_term(parser *p)
{
	ast_node *left = parse_unary(p);

	while (match_peek(p, TOKEN_STAR) || match_peek(p, TOKEN_SLASH)) {
		binary_op op = peek(p)->type == TOKEN_STAR ? OP_MUL : OP_DIV;
		advance(p);
		ast_node *right = parse_factor(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_BINARY;
		node->expr.binary.left = left;
		node->expr.binary.right = right;
		node->expr.binary.operator = op;
		left = node;
	}

	return left;
}

ast_node *parse_expression(parser *p)
{
	ast_node *left = parse_term(p);

	while (match_peek(p, TOKEN_PLUS) || match_peek(p, TOKEN_MINUS)) {
		binary_op op = peek(p)->type == TOKEN_PLUS ? OP_PLUS : OP_MINUS;
		advance(p);
		ast_node *right = parse_term(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_BINARY;
		node->expr.binary.left = left;
		node->expr.binary.right = right;
		node->expr.binary.operator = op;
		left = node;
	}

	return left;
}

static void parse(parser *p)
{
	p->ast = arena_alloc(p->allocator, sizeof(ast_node));
	p->ast->type = NODE_UNIT;
	p->ast->expr.unit_node.expr = parse_expression(p);
	ast_node *tail = p->ast;
	ast_node *expr = parse_expression(p);
	while (expr) {
		tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
		tail->expr.unit_node.next->expr.unit_node.expr = expr;
		tail = tail->expr.unit_node.next;
		tail->type = NODE_UNIT;
		expr = parse_expression(p);
	}
}

parser *parser_init(lexer *l, arena *allocator)
{
	parser *p = arena_alloc(allocator, sizeof(parser));
	p->tokens = l->tokens;
	p->allocator= allocator;

	parse(p);

	return p;
}
