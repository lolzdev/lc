#define STB_DS_IMPLEMENTATION
#include "sema.h"
#include <string.h>
#include <stdio.h>

typedef struct _res_node {
	struct _res_node **in;
	struct _res_node **out;
	type *value;
} res_node;

typedef struct { res_node node; bool complete; } pair;

typedef struct { u8 flags; char *name; } type_key;

static struct { char *key; pair *value; } *types;
static struct { char *key; type *value; } *type_reg;

/* Print the error message and sync the parser. */
static void error(ast_node *n, char *msg)
{
	if (n) {
		printf("\x1b[31m\x1b[1merror\x1b[0m\x1b[1m:%ld:%ld:\x1b[0m %s\n", n->position.row, n->position.column, msg);
	} else {
		printf("\x1b[31m\x1b[1merror\x1b[0m\x1b[1m:\x1b[0m %s\n", msg);
	}
}

static char *intern_string(sema *s, char *str, usize len)
{
	(void) s;
	char *ptr = malloc(len + 1);
	memcpy(ptr, str, len);
	ptr[len] = '\0';
	return ptr;
}

static type *create_integer(sema *s, char *name, u8 bits, bool sign)
{
	type *t = arena_alloc(s->allocator, sizeof(type));
	t->name = name;
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
	t->name = name;
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
		
		char *k = intern_string(s, node->expr.structure.name, node->expr.structure.name_len);
		t->name = k;
		pair *graph_node = shget(types, k);
		
		if (!graph_node) {
			graph_node = arena_alloc(s->allocator, sizeof(pair));
			graph_node->node.in = NULL;
			graph_node->node.out = NULL;
		} else if (graph_node->complete) {
			error(node, "type already defined.");
			return;
		}
		graph_node->node.value = t;

		member *m = t->data.structure.members;
		while (m) {
			if (m->type->type != NODE_IDENTIFIER) {
				m = m->next;
				continue;
			}
			char *name = intern_string(s, m->type->expr.string.start, m->type->expr.string.len);
			pair *p = shget(types, name);
			if (!p) {
				p = arena_alloc(s->allocator, sizeof(pair));
				p->node.out = NULL;
				p->node.in = NULL;
				p->node.value = NULL;
				p->complete = false;
				shput(types, name, p);
			}

			arrput(graph_node->node.in, &p->node);
			arrput(p->node.out, &graph_node->node);

			m = m->next;
		}

		shput(types, k, graph_node);
		graph_node->complete = true;
	}
}

static type *get_type(sema *s, ast_node *n)
{
	char *name = NULL;
	type *t = NULL;
	switch (n->type) {
		case NODE_IDENTIFIER:
			name = intern_string(s, n->expr.string.start, n->expr.string.len);
			t = shget(type_reg, name);
			free(name);
			return t;
		case NODE_PTR_TYPE:
			t = malloc(sizeof(type));
			t->size = sizeof(usize);
			t->alignment = sizeof(usize);
			if (n->expr.ptr_type.flags & PTR_RAW) {
				t->name = "ptr";
				t->tag = TYPE_PTR;
				t->data.ptr.child = get_type(s, n->expr.ptr_type.type);
				t->data.ptr.is_const = (n->expr.ptr_type.flags & PTR_CONST) != 0;
				t->data.ptr.is_volatile = (n->expr.ptr_type.flags & PTR_VOLATILE) != 0;
			} else {
				t->name = "slice";
				t->tag = TYPE_SLICE;
				t->data.slice.child = get_type(s, n->expr.ptr_type.type);
				t->data.slice.is_const = (n->expr.ptr_type.flags & PTR_CONST) != 0;
				t->data.slice.is_volatile = (n->expr.ptr_type.flags & PTR_VOLATILE) != 0;
			}
			return t;
		default:
			error(n, "expected type.");
			return NULL;
	}
}

static void register_struct(sema *s, char *name, type *t)
{
	usize alignment = 0;
	member *m = t->data.structure.members;

	usize offset = 0;
	type *m_type = NULL;
	while (m) {
		m_type = get_type(s, m->type);

		if (!m_type) {
			error(m->type, "unknown type.");
			return;
		}

		if (m_type->size == 0) {
			error(m->type, "a struct member can't be of type `void`.");
			return;
		}

		if (alignment < m_type->alignment) {
			alignment = m_type->alignment;
		}

		usize padding = (m_type->alignment - (offset % m_type->alignment)) % m_type->alignment;
		offset += padding;
		m->offset = offset;
		offset += m_type->size;

		m = m->next;
	}

	t->alignment = alignment;

	if (t->alignment > 0) {
		usize trailing_padding = (t->alignment - (offset % t->alignment)) % t->alignment;
		offset += trailing_padding;
	}

	t->size = offset;

	printf("%ld\n", t->size);
}

static void register_union(sema *s, char *name, type *t)
{
	usize alignment = 0;
	usize size = 0;
	member *m = t->data.structure.members;
	while (m) {
		type *m_type = get_type(s, m->type);
		if (alignment < m_type->alignment) {
			alignment = m_type->alignment;
		}

		if (size < m_type->size) {
			size = m_type->size;
		}
		
		m = m->next;
	}

	t->alignment = alignment;
	t->size = size;
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
		case TYPE_UNION:
			register_union(s, name, t);
			break;
		default:
			error(NULL, "registering an invalid type.");
			return;
	}

	shput(type_reg, name, t);
}

static void analyze_unit(sema *s, ast_node *node)
{
	ast_node *current = node;
	while (current && current->type == NODE_UNIT) {
		order_type(s, current->expr.unit_node.expr);
		current = current->expr.unit_node.next;
	}

	res_node **nodes = NULL;
	res_node **ordered = NULL;
	usize node_count = shlen(types);
	for (int i=0; i < node_count; i++) {
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

	if (arrlen(ordered) < node_count) {
		error(NULL, "cycling struct definition.");
	}

	for (int i=0; i < arrlen(ordered); i++) {
		type *t = ordered[i]->value;
		if (t && (t->tag == TYPE_STRUCT || t->tag == TYPE_UNION)) {
			char *name = t->name;
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

	register_type(s, "void", create_integer(s, "void", 0, false));
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


