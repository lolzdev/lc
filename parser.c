#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool has_errors = false;

ast_node *parse_expression(parser *p);

/* Consume a token in the list. */
static void advance(parser *p)
{
	p->previous = p->tokens;
	if (p->tokens)
		p->tokens = p->tokens->next;
}

/* Get the current token in the list, without consuming */
static token *peek(parser *p)
{
	return p->tokens;
}

/*
 * Check if the current token type is the same as `type`,
 * without consuming it.
 */
static bool match_peek(parser *p, token_type type)
{
	if (p->tokens) {
		return p->tokens->type == type;
	} else {
		return false;
	}
}

/* Same as `match_peek()` but it consumes the token. */
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

/*
 * When an error is encountered, try to find a
 * token that could define a part of the code
 * which doesn't depend on the one giving the
 * error. This is needed to print multiple errors
 * instead of just failing at the first one.
 */
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

/* Print the error message and sync the parser. */
static void error(parser *p, char *msg)
{
	printf("\x1b[31m\x1b[1merror\x1b[0m\x1b[1m:%ld:%ld:\x1b[0m %s\n", p->previous->position.row, p->previous->position.column, msg);
	has_errors = true;
	parser_sync(p);
}


static ast_node *parse_call(parser *p)
{
	ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
	node->type = NODE_CALL;
	node->expr.call.name = peek(p)->lexeme;
	node->expr.call.name_len = peek(p)->lexeme_len;
	advance(p);
	/* Skip also the opening `(` */
	advance(p);
	/* Call without parameters */
	if (match(p, TOKEN_RPAREN)) {
		node->expr.call.parameters = NULL;
		return node;
	}
	
	snapshot arena_start = arena_snapshot(p->allocator);
	node->expr.call.parameters = arena_alloc(p->allocator, sizeof(ast_node));
	node->expr.call.parameters->type = NODE_UNIT;
	node->expr.call.parameters->expr.unit_node.expr = parse_expression(p);
	ast_node *tail = node->expr.call.parameters;
	node->expr.call.param_len = 1;

	/* In this case, there is only one parameter */
	if (match(p, TOKEN_RPAREN)) {
		return node;
	}

	if (match(p, TOKEN_COMMA)) {
		ast_node *expr = parse_expression(p);
		if (expr) {
			while (!match(p, TOKEN_RPAREN)) {
				if (!match(p, TOKEN_COMMA)) {
					error(p, "expected `)`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
				tail->expr.unit_node.next->expr.unit_node.expr = expr;
				tail = tail->expr.unit_node.next;
				tail->type = NODE_UNIT;
				expr = parse_expression(p);
				if (!expr) {
					error(p, "expected `)`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				node->expr.call.param_len += 1;
			}

			tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
			tail->expr.unit_node.next->expr.unit_node.expr = expr;
			tail = tail->expr.unit_node.next;
			tail->type = NODE_UNIT;
		} else {
			error(p, "expected expression.");
			arena_reset_to_snapshot(p->allocator, arena_start);
			return NULL;
		}
	} else {
		error(p, "expected `)`.");
		arena_reset_to_snapshot(p->allocator, arena_start);
		return NULL;
	}

	return node;
}

/* Parse expressions with the highest precedence. */
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
	} else if (match_peek(p, TOKEN_IDENTIFIER)) {
		/* If a `(` is found after an identifier, it should be a call. */
		if (p->tokens->next && p->tokens->next->type == TOKEN_LPAREN) {
			return parse_call(p);
		}
		advance(p);

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

	/* Type cast. */
	if (match_peek(p, TOKEN_LPAREN) && p->tokens->next && p->tokens->next->type == TOKEN_IDENTIFIER && p->tokens->next->next && p->tokens->next->next->type == TOKEN_RPAREN) {
		advance(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_CAST;
		node->expr.cast.type = peek(p)->lexeme;
		node->expr.cast.type_len = peek(p)->lexeme_len;
		advance(p);
		advance(p);
		node->expr.cast.value = parse_expression(p);
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

/*
 * Following the recursive descent parser algorithm, this
 * parses all the arithmetic expressions.
 */
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

	/*
	 * If after parsing an expression a `[` character
	 * is found, it should be an array subscript expression.
	 */
	if (match(p, TOKEN_LSQUARE)) {
		ast_node *index = parse_expression(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_ARRAY_SUBSCRIPT;
		node->expr.subscript.expr = left;
		node->expr.subscript.index = index;

		if (!match(p, TOKEN_RSQUARE)) {
			error(p, "expected `]`.");
			return NULL;
		}
		return node;
	}

	/*
	 * If after parsing an expression a `.` character
	 * is found, it should be a member access expression.
	 */
	if (match(p, TOKEN_DOT)) {
		if (!match_peek(p, TOKEN_IDENTIFIER)) {
			error(p, "expected identifier after member access.");
			return NULL;
		}
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_ACCESS;
		node->expr.access.expr = left;
		node->expr.access.member = p->tokens->lexeme;
		node->expr.access.member_len = p->tokens->lexeme_len;
		advance(p);

		return node;
	}

	/*
	 * If after parsing an expression a `++` or a `--` 
	 * token is found, it should be a postfix expression.
	 */
	if (match(p, TOKEN_PLUS_PLUS) | match(p, TOKEN_MINUS_MINUS)) {
		unary_op op;
		switch (p->previous->type) {
			case TOKEN_PLUS_PLUS:
				op = UOP_INCR;
				break;
			case TOKEN_MINUS_MINUS:
				op = UOP_DECR;
				break;
			default:
				break;	
		}

		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_POSTFIX;
		node->expr.unary.operator = op;
		node->expr.unary.right = left;

		return node;
	}

	return left;
}

static ast_node *parse_statement(parser *p)
{
	if (match(p, TOKEN_BREAK)) {
		if (!match(p, TOKEN_SEMICOLON)) {
			error(p, "expected `;`.");
			return NULL;
		}
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_BREAK;
		return node;
	} else if (match(p, TOKEN_RETURN)) {
		ast_node *expr = parse_expression(p);

		if (!expr) {
			error(p, "expected expression.");
			return NULL;
		}
		if (!match(p, TOKEN_SEMICOLON)) {
			error(p, "expected `;`.");
			return NULL;
		}
		
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_RETURN;
		node->expr.ret.value = expr;
		return node;
	} else  {
		ast_node *expr = parse_expression(p);
		if (!expr) {
			return NULL;
		}
		if (!match(p, TOKEN_SEMICOLON)) {
			error(p, "expected `;`.");
			return NULL;
		}
		return expr;
	} 
}

/* Get a list of expressions to form a full AST. */
static void parse(parser *p)
{
	p->ast = arena_alloc(p->allocator, sizeof(ast_node));
	p->ast->type = NODE_UNIT;
	p->ast->expr.unit_node.expr = parse_statement(p);
	ast_node *tail = p->ast;
	ast_node *expr = parse_statement(p);
	while (expr) {
		tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
		tail->expr.unit_node.next->expr.unit_node.expr = expr;
		tail = tail->expr.unit_node.next;
		tail->type = NODE_UNIT;
		expr = parse_statement(p);
	}
}

parser *parser_init(lexer *l, arena *allocator)
{
	parser *p = arena_alloc(allocator, sizeof(parser));
	p->tokens = l->tokens;
	p->allocator= allocator;

	parse(p);

	if (has_errors) {
		printf("Compilation failed.\n");
		exit(1);
	}

	return p;
}
