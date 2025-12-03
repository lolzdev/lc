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
		case NODE_CAST:
			printf("Cast:\n");
			print_ast(node->expr.cast.type, depth);
			print_ast(node->expr.cast.value, depth + 1);
			break;
		case NODE_ACCESS:
			printf("Access:\n");
			print_ast(node->expr.access.expr, depth + 1);
			print_ast(node->expr.access.member, depth + 1);
			break;
		case NODE_LABEL:
			printf("Label: %.*s\n", (int)node->expr.label.name_len, node->expr.label.name);
			break;
		case NODE_GOTO:
			printf("Goto: %.*s\n", (int)node->expr.label.name_len, node->expr.label.name);
			break;
		case NODE_BINARY:
			printf("BinaryOp (%s)\n", get_op_str(node->expr.binary.operator));
			print_ast(node->expr.binary.left, depth + 1);
			print_ast(node->expr.binary.right, depth + 1);
			break;
		case NODE_ARRAY_SUBSCRIPT:
			printf("Array subscript\n");
			print_ast(node->expr.subscript.expr, depth + 1);
			print_ast(node->expr.subscript.index, depth + 1);
			break;
		case NODE_UNARY:
			printf("UnaryOp (%s)\n", get_uop_str(node->expr.unary.operator));
			print_ast(node->expr.unary.right, depth + 1);
			break;
		case NODE_POSTFIX:
			printf("Postfix (%s)\n", get_uop_str(node->expr.unary.operator));
			print_ast(node->expr.unary.right, depth + 1);
			break;
		case NODE_BREAK:
			printf("Break\n");
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
			printf("Unit\n");
			ast_node *current = node;
			while (current && current->type == NODE_UNIT) {
				print_ast(current->expr.unit_node.expr, depth + 1);
				current = current->expr.unit_node.next;
			}
			break;
		case NODE_COMPOUND: 
			printf("Block\n");
			for (usize i = 0; i < node->expr.compound.stmt_len; ++i) {
				print_ast(node->expr.compound.statements[i], depth + 1);
			}
			break;
		case NODE_CALL:
			printf("Call: %.*s\n", (int)node->expr.call.name_len, node->expr.call.name);
			current = node->expr.call.parameters;
			while (current && current->type == NODE_UNIT) {
				print_ast(current->expr.unit_node.expr, depth + 1);
				current = current->expr.unit_node.next;
			}
			break;
		case NODE_STRUCT_INIT:
			printf("Struct init:\n");
			current = node->expr.struct_init.members;
			while (current && current->type == NODE_UNIT) {
				print_ast(current->expr.unit_node.expr, depth + 1);
				current = current->expr.unit_node.next;
			}
			break;
		case NODE_STRUCT:
			printf("Struct: %.*s\n", (int)node->expr.structure.name_len, node->expr.structure.name);
			member *m = node->expr.structure.members;
			while (m) {
				print_ast(m->type, depth + 1);
				m = m->next;
			}
			break;
		case NODE_UNION:
			printf("Union: %.*s\n", (int)node->expr.structure.name_len, node->expr.structure.name);
			m = node->expr.structure.members;
			while (m) {
				print_ast(m->type, depth + 1);
				m = m->next;
			}
			break;
		case NODE_ENUM:
			printf("Enum: %.*s\n", (int)node->expr.enm.name_len, node->expr.enm.name);
			variant *v = node->expr.enm.variants;
			while (v) {
				printf("\t%.*s\n", (int)v->name_len, v->name);
				v = v->next;
			}
			break;
		case NODE_IF:
			printf("If:\n");
			print_ast(node->expr.whle.condition, depth + 1);
			print_ast(node->expr.whle.body, depth + 1);
			break;
		case NODE_VAR_DECL:
			printf("VarDecl: ");
			print_ast(node->expr.var_decl.type, 0);
			print_ast(node->expr.var_decl.value, depth + 1);
			break;
		case NODE_FUNCTION:
			printf("Function: %.*s\n", (int)node->expr.function.name_len, node->expr.function.name);
			m = node->expr.function.parameters;
			while (m) {
				print_ast(m->type, depth + 1);
				m = m->next;
			}
			print_ast(node->expr.function.body, depth + 1);
			break;
		case NODE_RETURN:
			printf("Return:\n");
			print_ast(node->expr.ret.value, depth + 1);
			break;
		case NODE_IMPORT:
			printf("Import:\n");
			print_ast(node->expr.import.path, depth + 1);
			break;
		case NODE_WHILE:
			printf("While:\n");
			print_ast(node->expr.whle.condition, depth + 1);
			print_ast(node->expr.whle.body, depth + 1);
			break;
		case NODE_FOR:
			printf("For:\n");
			print_ast(node->expr.fr.slices, depth + 1);
			print_ast(node->expr.fr.captures, depth + 1);
			print_indent(depth + 1);
			print_ast(node->expr.fr.body, depth + 1);
			break;
		case NODE_RANGE:
			printf("Range:\n");
			print_ast(node->expr.binary.left, depth + 1);
			print_ast(node->expr.binary.right, depth + 1);
			break;
		default:
			printf("Unknown Node Type: %d\n", node->type);
			break;
	}
}

int main(void)
{
	FILE *fp = fopen("examples/hello_world.l", "r");
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
