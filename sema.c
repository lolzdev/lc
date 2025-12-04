#define STB_DS_IMPLEMENTATION
#include "sema.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct _res_node {
	struct _res_node **in;
	struct _res_node **out;
	type *value;
} res_node;

typedef struct { res_node node; bool complete; } pair;

static struct { char *key; pair *value; } *types;
static struct { char *key; type *value; } *type_reg;

/* Print the error message and sync the parser. */
static void error(ast_node *n, char *msg)
{
	printf("\x1b[31m\x1b[1merror\x1b[0m\x1b[1m:%ld:%ld:\x1b[0m %s\n", n->position.row, n->position.column, msg);
}

static char *intern_string(sema *s, char *str, usize len)
{
	char *ptr = arena_alloc(s->allocator, len + 1);
	memcpy(ptr, str, len);
	ptr[len] = '\0';
	return ptr;
}

static type *create_integer(sema *s, char *name, u8 bits, bool sign)
{
	type *t = arena_alloc(s->allocator, sizeof(type));
	t->tag = sign ? TYPE_INTEGER : TYPE_UINTEGER;
	t->data.integer = bits;
	
	pair *graph_node = arena_alloc(s->allocator, sizeof(pair));
	graph_node->node.value = t;
	graph_node->node.in = NULL;
	graph_node->node.out = NULL;

	shput(types, name, graph_node);
	return t;
}

static type *create_float(sema *s, char *name, u8 bits)
{
	type *t = arena_alloc(s->allocator, sizeof(type));
	t->tag = TYPE_FLOAT;
	t->data.flt = bits;
	
	pair *graph_node = arena_alloc(s->allocator, sizeof(pair));
	graph_node->node.value = t;
	graph_node->node.in = NULL;
	graph_node->node.out = NULL;

	shput(types, name, graph_node);
	return t;
}

/* https://en.wikipedia.org/wiki/Topological_sorting */
static void order_type(sema *s, ast_node *node)
{
	if (node->type == NODE_STRUCT || node->type == NODE_UNION) {
		type *t = arena_alloc(s->allocator, sizeof(type));
		t->tag = node->type == NODE_STRUCT ? TYPE_STRUCT : TYPE_UNION;
		t->data.structure.name = node->expr.structure.name;
		t->data.structure.name_len = node->expr.structure.name_len;
		t->data.structure.members = node->expr.structure.members;
		
		pair *graph_node = shget(types, intern_string(s, node->expr.structure.name, node->expr.structure.name_len));
		
		if (!graph_node) {
			graph_node = arena_alloc(s->allocator, sizeof(pair));
			graph_node->node.value = t;
			graph_node->node.in = NULL;
			graph_node->node.out = NULL;
		} else {
			graph_node->complete = true;
		}

		member *m = t->data.structure.members;
		while (m) {
			char *name = intern_string(s, m->name, m->name_len);
			pair *p = shget(types, name);
			if (!p) {
				p = arena_alloc(s->allocator, sizeof(pair));
				p->complete = false;
				p->node.out = NULL;
				p->node.in = NULL;
				shput(types, name, p);
			}

			arrput(p->node.in, &graph_node->node);
			arrput(graph_node->node.out, &p->node);

			m = m->next;
		}

		shput(types, intern_string(s, t->data.structure.name, t->data.structure.name_len), graph_node);
	}
}

static void register_struct(sema *s, char *name, type *t)
{

}

static void register_union(sema *s, char *name, type *t)
{

}

static void register_type(sema *s, char *name, type *t)
{
	switch (t->tag) {
		case TYPE_INTEGER:
		case TYPE_UINTEGER:
			t->size = t->data.integer / 8;
			t->alignment = t->data.integer / 8;
			break;
		case TYPE_PTR:
			t->size = 8;
			t->alignment = 8;
			break;
		case TYPE_FLOAT:
			t->size = t->data.flt / 8;
			t->alignment = t->data.flt / 8;
			break;
		case TYPE_STRUCT:
			register_struct(s, name, t);
			break;
	}

	shput(type_reg, name, t);
}

static void analyze_unit(sema *s, ast_node *node)
{
	ast_node *current = node;
	while (current && current->type == NODE_UNIT) {
		order_type(s, current->expr.unit_node.expr);
end:
		current = current->expr.unit_node.next;
	}

	res_node **nodes = NULL;
	res_node **ordered = NULL;
	for (int i=0; i < shlen(types); i++) {
		if (arrlen(types[i].value->node.in) == 0) {
			arrput(nodes, &types[i].value->node);
		}
	}

	while (arrlen(nodes) > 0) {
		res_node *n = nodes[0];
		arrdel(nodes, 0);
		arrput(ordered, n);
		while (arrlen(n->out) > 0) {
			res_node *dep = n->out[0];
			arrdel(n->out, 0);
			
			for (int j=0; j < arrlen(dep->in); j++) {
				if (dep->in[j] == n) {
					arrdel(dep->in, j);
				}
			}

			if (arrlen(dep->in) == 0) {
				arrput(nodes, dep);
			}
		}
	}

	for (int i=0; i < arrlen(ordered); i++) {
		type *t = ordered[i]->value;
		if (t && (t->tag == TYPE_STRUCT || t->tag == TYPE_UNION)) {
			char *name = intern_string(s, t->data.structure.name, t->data.structure.name_len);
			register_type(s, name, t);
		}
	}
}

sema *sema_init(parser *p, arena *a)
{
	sema *s = arena_alloc(a, sizeof(sema));
	s->allocator = a;
	types = NULL;
	s->ast = p->ast;

	register_type(s, "u8", create_integer(s, "u8", 8, false));
	register_type(s, "u16", create_integer(s, "u16", 16, false));
	register_type(s, "u32", create_integer(s, "u32", 32, false));
	register_type(s, "u64", create_integer(s, "u64", 64, false));
	register_type(s, "i8", create_integer(s, "i8", 8, true));
	register_type(s, "i16", create_integer(s, "i16", 16, true));
	register_type(s, "i32", create_integer(s, "i32", 32, true));
	register_type(s, "i64", create_integer(s, "i64", 64, true));
	register_type(s, "f32", create_float(s, "f32", 32));
	register_type(s, "f64", create_float(s, "f64", 64));

	analyze_unit(s, s->ast);

	return s;
}


