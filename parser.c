#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool has_errors = false;

ast_node *parse_expression(parser *p);
static ast_node *parse_statement(parser *p);

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
	if (p->tokens)
	{
		return p->tokens->type == type;
	}
	else
	{
		return false;
	}
}

/* Same as `match_peek()` but it consumes the token. */
static bool match(parser *p, token_type type)
{
	if (p->tokens)
	{
		if (p->tokens->type == type)
		{
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

	while (p->tokens)
	{
		if (p->previous->type == TOKEN_SEMICOLON || p->previous->type == TOKEN_RCURLY)
		{
			return;
		}

		switch (p->tokens->type)
		{
		case TOKEN_STRUCT:
		case TOKEN_ENUM:
		case TOKEN_IF:
		case TOKEN_LOOP:
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
	if (match(p, TOKEN_RPAREN))
	{
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
	if (match(p, TOKEN_RPAREN))
	{
		return node;
	}

	if (match(p, TOKEN_COMMA))
	{
		ast_node *expr = parse_expression(p);
		if (expr)
		{
			while (!match(p, TOKEN_RPAREN))
			{
				if (!match(p, TOKEN_COMMA))
				{
					error(p, "expected `)`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
				tail->expr.unit_node.next->expr.unit_node.expr = expr;
				tail = tail->expr.unit_node.next;
				tail->type = NODE_UNIT;
				expr = parse_expression(p);
				if (!expr)
				{
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
		}
		else
		{
			error(p, "expected expression.");
			arena_reset_to_snapshot(p->allocator, arena_start);
			return NULL;
		}
	}
	else
	{
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
	if (match(p, TOKEN_INTEGER))
	{
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_INTEGER;
		node->expr.integer = parse_int(t->lexeme, t->lexeme_len);
		if (match(p, TOKEN_DOUBLE_DOT)) {
			ast_node *range = arena_alloc(p->allocator, sizeof(ast_node));
			range->type = NODE_RANGE;
			range->expr.binary.left = node;
			range->expr.binary.operator = OP_PLUS;
			snapshot snap = arena_snapshot(p->allocator);
			ast_node *end = parse_factor(p);
			if (!end) {
				range->expr.binary.right = NULL;
			} else if (end->type != NODE_INTEGER) {
				arena_reset_to_snapshot(p->allocator, snap);
				error(p, "expected integer.");
				return NULL;
			} else {
				range->expr.binary.right = end;
			}
			return range;
		}
		return node;
	}
	else if (match(p, TOKEN_FLOAT))
	{
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_FLOAT;
		node->expr.flt = parse_float(t->lexeme, t->lexeme_len);
		return node;
	}
	else if (match_peek(p, TOKEN_IDENTIFIER))
	{
		/* If a `(` is found after an identifier, it should be a call. */
		if (p->tokens->next && p->tokens->next->type == TOKEN_LPAREN)
		{
			return parse_call(p);
		}
		advance(p);

		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_IDENTIFIER;
		node->expr.string.start = t->lexeme;
		node->expr.string.len = t->lexeme_len;
		return node;
	}
	else if (match(p, TOKEN_STRING))
	{
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_STRING;
		node->expr.string.start = t->lexeme;
		node->expr.string.len = t->lexeme_len;
		return node;
	}
	else if (match(p, TOKEN_CHAR))
	{
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_CHAR;
		if (t->lexeme_len == 2)
		{
			char c;
			switch (t->lexeme[1])
			{
			case 'n':
				c = '\n';
				break;
			case 't':
				c = '\t';
				break;
			case 'r':
				c = '\r';
				break;
			case '0':
				c = '\0';
				break;
			case '\\':
				c = '\\';
				break;
			case '\'':
				c = '\'';
				break;
			default:
				error(p, "invalid escape code.");
				return NULL;
			}
			node->expr.ch = c;
		}
		else
		{
			node->expr.ch = *(t->lexeme);
		}
		return node;
	}
	else if (match(p, TOKEN_LPAREN))
	{
		ast_node *node = parse_expression(p);
		if (!match(p, TOKEN_RPAREN))
		{
			error(p, "unclosed parenthesis");
			return NULL;
		}

		return node;
	}

	return NULL;
}

ast_node *parse_unary(parser *p)
{
	if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS) || match(p, TOKEN_MINUS_MINUS) || match(p, TOKEN_STAR) || match(p, TOKEN_AND) || match(p, TOKEN_BANG))
	{
		unary_op op;
		switch (p->previous->type)
		{
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
	if (match_peek(p, TOKEN_LPAREN) && p->tokens->next && p->tokens->next->type == TOKEN_IDENTIFIER && p->tokens->next->next && p->tokens->next->next->type == TOKEN_RPAREN)
	{
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

	while (match_peek(p, TOKEN_STAR) || match_peek(p, TOKEN_SLASH))
	{
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

	while (match_peek(p, TOKEN_PLUS) || match_peek(p, TOKEN_MINUS))
	{
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
	if (match(p, TOKEN_LSQUARE))
	{
		ast_node *index = parse_expression(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_ARRAY_SUBSCRIPT;
		node->expr.subscript.expr = left;
		node->expr.subscript.index = index;

		if (!match(p, TOKEN_RSQUARE))
		{
			error(p, "expected `]`.");
			return NULL;
		}
		return node;
	}

	/*
	 * If after parsing an expression a `.` character
	 * is found, it should be a member access expression.
	 */
	if (match(p, TOKEN_DOT))
	{
		if (!match_peek(p, TOKEN_IDENTIFIER))
		{
			error(p, "expected identifier after member access.");
			return NULL;
		}
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_ACCESS;
		node->expr.access.expr = left;
		node->expr.access.member = parse_expression(p);

		return node;
	}

	/*
	 * If after parsing an expression a `++` or a `--`
	 * token is found, it should be a postfix expression.
	 */
	if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS_MINUS))
	{
		unary_op op;
		switch (p->previous->type)
		{
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

	if ((p->tokens->type >= TOKEN_DOUBLE_EQ && p->tokens->type <= TOKEN_NOT_EQ) || (p->tokens->type >= TOKEN_LSHIFT_EQ && p->tokens->type <= TOKEN_DOUBLE_AND))
	{
		binary_op op;
		switch (p->tokens->type)
		{
		case TOKEN_DOUBLE_EQ:
			op = OP_EQ;
			break;
		case TOKEN_LESS_THAN:
			op = OP_LT;
			break;
		case TOKEN_GREATER_THAN:
			op = OP_GT;
			break;
		case TOKEN_LESS_EQ:
			op = OP_LE;
			break;
		case TOKEN_GREATER_EQ:
			op = OP_GE;
			break;
		case TOKEN_NOT_EQ:
			op = OP_NEQ;
			break;
		case TOKEN_LSHIFT_EQ:
			op = OP_LSHIFT_EQ;
			break;
		case TOKEN_RSHIFT_EQ:
			op = OP_RSHIFT_EQ;
			break;
		case TOKEN_OR:
			op = OP_OR;
			break;
		case TOKEN_DOUBLE_AND:
			op = OP_AND;
			break;
		default:
			break;
		}
		advance(p);
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_BINARY;
		node->expr.binary.left = left;
		node->expr.binary.operator = op;
		node->expr.binary.right = parse_expression(p);

		return node;
	}

	return left;
}

static ast_node *parse_compound(parser *p)
{
	if (!match(p, TOKEN_LCURLY))
	{
		error(p, "expected `{` for beginning of a block.");
		return NULL;
	}
	ast_node *compound = arena_alloc(p->allocator, sizeof(ast_node));
	compound->type = NODE_UNIT;
	compound->expr.unit_node.expr = NULL;
    compound->expr.unit_node.next = NULL;
	ast_node* tail = compound;
	// FIXME: This only works with correct blocks, incorrect blocks segfault
	while (p->tokens->type != TOKEN_RCURLY &&
           p->tokens->type != TOKEN_END)
	{
		ast_node* stmt = parse_statement(p);
		tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
		tail->expr.unit_node.next->expr.unit_node.expr = stmt;
		tail = tail->expr.unit_node.next;
		tail->type = NODE_UNIT;
	}
	if (p->tokens->type != TOKEN_RCURLY) {
		error(p, "Unterminated block.");
		return NULL;
	}
	return compound;
}

static ast_node *parse_for(parser *p)
{
	advance(p);
	ast_node* node = arena_alloc(p->allocator, sizeof(ast_node));
	node->type = NODE_FOR;

	snapshot arena_start = arena_snapshot(p->allocator);
	node->expr.fr.slices = arena_alloc(p->allocator, sizeof(ast_node));
	node->expr.fr.slices->type = NODE_UNIT;
	node->expr.fr.slices->expr.unit_node.expr = parse_expression(p);
	ast_node *tail = node->expr.fr.slices;
	node->expr.fr.slice_len = 1;

	/* In this case, there is only one slice. */
	if (match(p, TOKEN_RPAREN))
	{
		goto parse_captures;
	}

	if (match(p, TOKEN_COMMA))
	{
		ast_node *expr = parse_expression(p);
		if (expr)
		{
			while (!match(p, TOKEN_RPAREN))
			{
				if (!match(p, TOKEN_COMMA))
				{
					error(p, "expected `)`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
				tail->expr.unit_node.next->expr.unit_node.expr = expr;
				tail = tail->expr.unit_node.next;
				tail->type = NODE_UNIT;
				expr = parse_expression(p);
				if (!expr)
				{
					error(p, "expected `)`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				node->expr.fr.slice_len += 1;
			}

			tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
			tail->expr.unit_node.next->expr.unit_node.expr = expr;
			tail = tail->expr.unit_node.next;
			tail->type = NODE_UNIT;
		}
		else
		{
			error(p, "expected expression.");
			arena_reset_to_snapshot(p->allocator, arena_start);
			return NULL;
		}
	}
	else
	{
		error(p, "expected `)`.");
		arena_reset_to_snapshot(p->allocator, arena_start);
		return NULL;
	}

parse_captures:

	if (!match(p, TOKEN_PIPE)) {
		error(p, "expected capture.");
		return NULL;
	}

	arena_start = arena_snapshot(p->allocator);
	node->expr.fr.captures = arena_alloc(p->allocator, sizeof(ast_node));
	node->expr.fr.captures->type = NODE_UNIT;
	node->expr.fr.captures->expr.unit_node.expr = parse_expression(p);
	if (node->expr.fr.captures->expr.unit_node.expr && node->expr.fr.captures->expr.unit_node.expr->type != NODE_IDENTIFIER) {
		error(p, "captures must be identifiers.");
		arena_reset_to_snapshot(p->allocator, arena_start);
		return NULL;
	}
	tail = node->expr.fr.captures;
	node->expr.fr.capture_len = 1;

	/* In this case, there is only one capture */
	if (match(p, TOKEN_PIPE)) {
		goto parse_body;
	}

	if (match(p, TOKEN_COMMA)) {
		ast_node *expr = parse_expression(p);
		if (expr) {
			while (!match(p, TOKEN_PIPE)) {
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
					error(p, "expected `|`.");
					arena_reset_to_snapshot(p->allocator, arena_start);
					return NULL;
				}
				node->expr.fr.capture_len += 1;
			}

			tail->expr.unit_node.next = arena_alloc(p->allocator, sizeof(ast_node));
			tail->expr.unit_node.next->expr.unit_node.expr = expr;
			tail = tail->expr.unit_node.next;
			tail->type = NODE_UNIT;
		} else {
			error(p, "expected identifier.");
			arena_reset_to_snapshot(p->allocator, arena_start);
			return NULL;
		}
	} else {
		error(p, "expected `|`.");
		arena_reset_to_snapshot(p->allocator, arena_start);
		return NULL;
	}

parse_body:;
	if (node->expr.fr.capture_len != node->expr.fr.slice_len) {
		error(p, "invalid number of captures.");
		return NULL;
	}

	ast_node* body = parse_compound(p);
	node->expr.fr.body = body;
	return node;
}

static ast_node *parse_while(parser *p)
{
	ast_node *condition = parse_expression(p);
	ast_node *body = parse_compound(p);
	ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
	node->type = NODE_WHILE;
	node->expr.whle.body = body;
	node->expr.whle.condition = condition;
	return node;
}

static ast_node *parse_statement(parser *p)
{
	if (match(p, TOKEN_BREAK))
	{
		if (!match(p, TOKEN_SEMICOLON))
		{
			error(p, "expected `;` after `break`.");
			return NULL;
		}
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_BREAK;
		return node;
	}
	else if (match(p, TOKEN_RETURN))
	{
		ast_node *expr = parse_expression(p);

		if (!expr)
		{
			error(p, "expected expression after `return`.");
			return NULL;
		}
		if (!match(p, TOKEN_SEMICOLON))
		{
			error(p, "expected `;`.");
			return NULL;
		}

		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_RETURN;
		node->expr.ret.value = expr;
		return node;
	}
	else if (match_peek(p, TOKEN_IDENTIFIER) && p->tokens->next && p->tokens->next->type == TOKEN_COLON)
	{
		/* In this case, this is a label. */
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_LABEL;
		node->expr.label.name = p->tokens->lexeme;
		node->expr.label.name_len = p->tokens->lexeme_len;
		advance(p);
		/* Consume `:` */
		advance(p);
		return node;
	}
	else if (match(p, TOKEN_GOTO))
	{
		if (!match_peek(p, TOKEN_IDENTIFIER))
		{
			error(p, "expected label identifier after `goto`.");
			return NULL;
		}
		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_GOTO;
		node->expr.label.name = p->tokens->lexeme;
		node->expr.label.name_len = p->tokens->lexeme_len;
		advance(p);
		if (!match(p, TOKEN_SEMICOLON))
		{
			error(p, "expected `;` after `goto`.");
			return NULL;
		}
		return node;
	}
	else if (match(p, TOKEN_IMPORT))
	{
		ast_node *expr = parse_expression(p);
		if (!expr)
		{
			error(p, "expected module path after `import`.");
			return NULL;
		}
		if (expr->type != NODE_ACCESS && expr->type != NODE_IDENTIFIER)
		{
			error(p, "expected module path after `import`.");
			return NULL;
		}

		ast_node *node = arena_alloc(p->allocator, sizeof(ast_node));
		node->type = NODE_IMPORT;
		node->expr.import.path = expr;

		if (!match(p, TOKEN_SEMICOLON))
		{
			error(p, "expected `;` after `import`.");
			return NULL;
		}

		return node;
	}
	else if (match(p, TOKEN_LOOP))
	{
		if (p->tokens->type == TOKEN_LPAREN)
		{
			return parse_for(p);
		}
		else
		{
			return parse_while(p);
		}
	}
	else
	{
		ast_node *expr = parse_expression(p);
		if (!expr)
		{
			return NULL;
		}
		if (!match(p, TOKEN_SEMICOLON))
		{
			error(p, "expected `;` after expression.");
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
