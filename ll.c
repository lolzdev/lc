#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "lexer.h"
#include "parser.h"

void print_indent(int depth) {
	for (int i = 0; i < depth; i++) printf("  ");
}

const char* get_op_str(binary_op op) {
	switch(op) {
		case OP_PLUS: return "+";
		case OP_MINUS: return "-";
		case OP_DIV: return "/";
		case OP_MUL: return "*";
		case OP_EQ: return "==";
		case OP_ASSIGN: return "=";
		case OP_AND: return "&&";
		case OP_OR: return "||";
		case OP_NEQ: return "!=";
		case OP_GT: return ">";
		case OP_LT: return "<";
		case OP_GE: return ">=";
		case OP_LE: return "<=";
		case OP_BOR: return "|";
		case OP_BAND: return "&";
		case OP_BXOR: return "^";
		case OP_MOD: return "%";
		case OP_PLUS_EQ: return "+=";
		case OP_MINUS_EQ: return "-=";
		case OP_DIV_EQ: return "/=";
		case OP_MUL_EQ: return "*=";
		default: return "?";
	}
}

const char *get_uop_str(unary_op op) {
	switch (op) {
		case UOP_INCR: return "++";
		case UOP_MINUS: return "-";
		case UOP_DECR: return "--";
		case UOP_DEREF: return "*";
		case UOP_REF: return "&";
		case UOP_NOT: return "!";
		default: return "?";
	}
}

void print_ast(ast_node *node, int depth) {
	if (!node) return;

	print_indent(depth);

	switch (node->type) {
		case NODE_INTEGER:
			printf("Integer: %lu\n", node->expr.integer);
			break;
		case NODE_FLOAT:
			printf("Float: %f\n", node->expr.flt);
			break;
		case NODE_CHAR:
			printf("Char: '%c'\n", node->expr.ch);
			break;
		case NODE_STRING:
			printf("String: \"%.*s\"\n", (int)node->expr.string.len, node->expr.string.start);
			break;
		case NODE_IDENTIFIER:
			printf("Identifier: %.*s\n", (int)node->expr.string.len, node->expr.string.start);
			break;
		case NODE_BINARY:
			printf("BinaryOp (%s)\n", get_op_str(node->expr.binary.operator));
			print_ast(node->expr.binary.left, depth + 1);
			print_ast(node->expr.binary.right, depth + 1);
			break;
		case NODE_UNARY:
			printf("UnaryOp (%s)\n", get_uop_str(node->expr.unary.operator));
			print_ast(node->expr.unary.right, depth + 1);
			break;
		case NODE_TERNARY:
			printf("Ternary (? :)\n");
			print_indent(depth + 1); printf("Condition:\n");
			print_ast(node->expr.ternary.condition, depth + 2);
			print_indent(depth + 1); printf("Then:\n");
			print_ast(node->expr.ternary.then, depth + 2);
			print_indent(depth + 1); printf("Else:\n");
			print_ast(node->expr.ternary.otherwise, depth + 2);
			break;
		case NODE_UNIT:
		case NODE_COMPOUND: 
			printf("Unit/Block:\n");
			ast_node *current = node;
			while (current && (current->type == NODE_UNIT || current->type == NODE_COMPOUND)) {
				print_ast(current->expr.unit_node.expr, depth + 1);
				current = current->expr.unit_node.next;
			}
			break;
		case NODE_IF:
			printf("IfStmt (Fields missing in struct)\n");
			break;
		case NODE_WHILE:
			printf("WhileStmt (Fields missing in struct)\n");
			break;
		case NODE_VAR_DECL:
			printf("VarDecl (Fields missing in struct)\n");
			break;
		case NODE_FUNCTION_DEF:
			printf("FunctionDef (Fields missing in struct)\n");
			break;
		case NODE_RETURN:
			printf("Return (Fields missing in struct)\n");
			break;
		default:
			printf("Unknown Node Type: %d\n", node->type);
			break;
	}
}

int main(void)
{
	FILE *fp = fopen("test.c", "r");
	usize size = 0;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = malloc(size+1);
	fread(src, size, 1, fp);
	fclose(fp);
	src[size] = '\0';

	arena a = arena_init(0x1000 * 0x1000 * 64);
	lexer *l = lexer_init(src, size, &a);
	parser *p = parser_init(l, &a);
	print_ast(p->ast, 0);

	arena_deinit(a);

	return 0;
}
