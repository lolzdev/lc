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

static struct { char *key; prototype *value; } *prototypes;

static scope *global_scope = NULL;
static scope *current_scope = NULL;
static type *current_return = NULL;

static bool in_loop = false;

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

		char *n = intern_string(s, m->name, m->name_len);
		shput(t->data.structure.member_types, n, m_type);

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
}

static void register_union(sema *s, char *name, type *t)
{
	usize alignment = 0;
	usize size = 0;
	member *m = t->data.structure.members;
	while (m) {
		type *m_type = get_type(s, m->type);
		
		if (!m_type) {
			error(m->type, "unknown type.");
			return;
		}

		char *n = intern_string(s, m->name, m->name_len);
		shput(t->data.structure.member_types, n, m_type);

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

static void create_types(sema *s)
{
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

static void create_prototype(sema *s, ast_node *node)
{
	prototype *p = arena_alloc(s->allocator, sizeof(prototype));
	p->name = intern_string(s, node->expr.function.name, node->expr.function.name_len);
	if (shget(prototypes, p->name)) {
		error(node, "function already defined.");
	}

	member *m = node->expr.function.parameters;
	while (m) {
		type *t = get_type(s, m->type);
		if (!t) {
			error(m->type, "unknown type.");
			return;
		}

		arrput(p->parameters, t);
		m = m->next;
	}

	p->type = get_type(s, node->expr.function.type);
	shput(prototypes, p->name, p);
}

static void push_scope(sema *s)
{
	scope *scp = arena_alloc(s->allocator, sizeof(scope));
	scp->parent = current_scope;
	current_scope = scp;
}

static void pop_scope(sema *s)
{
	current_scope = current_scope->parent;
}

static type *get_def(sema *s, char *name)
{
	scope *current = current_scope;
	while (current) {
		type *t = shget(current->defs, name);
		if (t) return t;

		current = current->parent;
	}

	return NULL;
}

static type *get_string_type(sema *s, ast_node *node)
{
	type *string_type = arena_alloc(s->allocator, sizeof(type));
	string_type->tag = TYPE_PTR;
	string_type->size = sizeof(usize);
	string_type->alignment = sizeof(usize);
	string_type->name = "slice";
	string_type->data.slice.child = shget(type_reg, "u8");
	string_type->data.slice.is_const = true;
	string_type->data.slice.is_volatile = false;
	string_type->data.slice.len = node->expr.string.len;
	return string_type;
}

static type *get_range_type(sema *s, ast_node *node)
{
	type *range_type = arena_alloc(s->allocator, sizeof(type));
	range_type->tag = TYPE_PTR;
	range_type->size = sizeof(usize);
	range_type->alignment = sizeof(usize);
	range_type->name = "slice";
	range_type->data.slice.child = shget(type_reg, "usize");
	range_type->data.slice.is_const = true;
	range_type->data.slice.is_volatile = false;
	range_type->data.slice.len = node->expr.binary.right->expr.integer - node->expr.binary.left->expr.integer;
	return range_type;
}

static type *get_expression_type(sema *s, ast_node *node);
static type *get_access_type(sema *s, ast_node *node)
{
	type *t = get_expression_type(s, node->expr.access.expr);
	ast_node *member = node->expr.access.member;
	char *name_start = member->expr.string.start;
	usize name_len = member->expr.string.len;
	if (!t || (t->tag != TYPE_STRUCT && t->tag != TYPE_UNION)) {
		error(node, "invalid expression.");
		return NULL;
	}
	char *name = intern_string(s, name_start, name_len);
	type *res = shget(t->data.structure.member_types, name);
	if (!res) {
		error(node, "struct doesn't have that member");
		return NULL;
	}

	return res;
}

static type *get_identifier_type(sema *s, ast_node *node)
{
	char *name_start = node->expr.string.start;
	usize name_len = node->expr.string.len;
	type *t = get_def(s, intern_string(s, name_start, name_len));
	if (!t) {
		error(node, "unknown identifier.");
	}
	return t;
}

static bool match(type *t1, type *t2);

static type *get_expression_type(sema *s, ast_node *node)
{
	if (!node) {
		return shget(type_reg, "void");
	}

	type *t = NULL;
	prototype *prot = NULL;
	switch (node->type) {
		case NODE_IDENTIFIER:
			return get_identifier_type(s, node);
		case NODE_INTEGER:
			return shget(type_reg, "i32");
		case NODE_FLOAT:
			return shget(type_reg, "f64");
		case NODE_STRING:
			return get_string_type(s, node);
		case NODE_CHAR:
			return shget(type_reg, "u8");
		case NODE_BOOL:
			return shget(type_reg, "bool");
		case NODE_CAST:
			return get_type(s, node->expr.cast.type);
		case NODE_POSTFIX:
		case NODE_UNARY:
			return get_expression_type(s, node->expr.unary.right);
		case NODE_BINARY:
			t = get_expression_type(s, node->expr.binary.left);
			if (!t) return NULL;
			if (!match(t, get_expression_type(s, node->expr.binary.right))) {
				error(node, "type mismatch.");
				return NULL;
			}
			if (node->expr.binary.operator >= OP_EQ) {
				return shget(type_reg, "bool");
			} else if (node->expr.binary.operator >= OP_ASSIGN && node->expr.binary.operator <= OP_MOD_EQ) {
				return shget(type_reg, "void");
			} else {
				return t;
			}
		case NODE_RANGE:
			return get_range_type(s, node);
		case NODE_ARRAY_SUBSCRIPT:
			t = get_expression_type(s, node->expr.subscript.expr);
			switch (t->tag) {
				case TYPE_SLICE:
					return t->data.slice.child;
				case TYPE_PTR:
					return t->data.ptr.child;
				default:
					error(node, "only pointers and slices can be indexed.");
					return NULL;
			}
		case NODE_CALL:
			prot = shget(prototypes, intern_string(s, node->expr.call.name, node->expr.call.name_len));
			if (!prot) {
				error(node, "unknown function.");
				return NULL;
			}
			return prot->type;
		case NODE_ACCESS:
			return get_access_type(s, node);
		default:
			return shget(type_reg, "void");
	}
}

static bool match(type *t1, type *t2)
{
	if (!t1 || !t2) return false;
	if (t1->tag != t2->tag) return false;

	switch(t1->tag) {
		case TYPE_VOID:
		case TYPE_BOOL:
			return true;
		case TYPE_PTR:
			return (t1->data.ptr.is_const == t2->data.ptr.is_const) && (t1->data.ptr.is_volatile == t2->data.ptr.is_volatile) && match(t1->data.ptr.child, t2->data.ptr.child);
		case TYPE_SLICE:
			return (t1->data.slice.is_const == t2->data.slice.is_const) && (t1->data.slice.is_volatile == t2->data.slice.is_volatile) && match(t1->data.slice.child, t2->data.slice.child) && t1->data.slice.len == t2->data.slice.len;
		case TYPE_STRUCT:
		case TYPE_UNION:
			return t1 == t2;
		case TYPE_INTEGER:
		case TYPE_UINTEGER:
			return t1->data.integer == t2->data.integer;
		case TYPE_FLOAT:
			return t1->data.flt == t2->data.flt;
		case TYPE_ENUM:
		case TYPE_GENERIC:
			/* TODO */
			return false;
	}

	return false;
}

static void check_statement(sema *s, ast_node *node);
static void check_body(sema *s, ast_node *node)
{
	push_scope(s);

	ast_node *current = node;
	while (current && current->type == NODE_UNIT) {
		check_statement(s, current->expr.unit_node.expr);
		current = current->expr.unit_node.next;
	}

	pop_scope(s);
}

static void check_for(sema *s, ast_node *node)
{
	ast_node *slices = node->expr.fr.slices;
	ast_node *captures = node->expr.fr.captures;

	push_scope(s);

	ast_node *current_capture = captures;
	ast_node *current_slice = slices;

	while (current_capture) {
		type *c_type = get_expression_type(s, current_slice->expr.unit_node.expr);
		char *c_name = intern_string(s, current_capture->expr.unit_node.expr->expr.string.start, current_capture->expr.unit_node.expr->expr.string.len);
		shput(current_scope->defs, c_name, c_type);
		current_capture = current_capture->expr.unit_node.next;
		current_slice = current_slice->expr.unit_node.next;
	}

	ast_node *current = node->expr.fr.body;

	in_loop = true;
	while (current && current->type == NODE_UNIT) {
		check_statement(s, current->expr.unit_node.expr);
		current = current->expr.unit_node.next;
	}
	in_loop = false;

	pop_scope(s);
}

static void check_statement(sema *s, ast_node *node)
{
	if (!node) return;

	type *t = NULL;
	char *name = NULL;
	switch(node->type) {
		case NODE_RETURN:
			if (!match(get_expression_type(s, node->expr.ret.value), current_return)) {
				error(node, "return type doesn't match function's one.");
			}
			break;
		case NODE_BREAK:
			if (!in_loop) {
				error(node, "`break` isn't in a loop.");
			}
			break;
		case NODE_WHILE:
			if (!match(get_expression_type(s, node->expr.whle.condition), shget(type_reg, "bool"))) {
				error(node, "expected boolean value.");
				return;
			}

			in_loop = true;
			check_body(s, node->expr.whle.body);
			in_loop = false;
			break;
		case NODE_FOR:
			check_for(s, node);
			break;
		case NODE_VAR_DECL:
			t = get_type(s, node->expr.var_decl.type);
			name = intern_string(s, node->expr.var_decl.name, node->expr.var_decl.name_len);
			if (get_def(s, name)) {
				error(node, "redeclaration of variable.");
				break;
			}
			if (!match(t, get_expression_type(s, node->expr.var_decl.value))) {
				error(node, "type mismatch.");
			}
			shput(current_scope->defs, name, t);
			break;
		default:
			get_expression_type(s, node);
			break;
	}
}

static void check_function(sema *s, ast_node *f)
{
	push_scope(s);
	current_return = get_type(s, f->expr.function.type);

	member *param = f->expr.function.parameters;
	while (param) {
		type *p_type = get_type(s, param->type);
		char *t_name = intern_string(s, param->name, param->name_len);
		shput(current_scope->defs, t_name, p_type);
		param = param->next;
	}

	ast_node *current = f->expr.function.body;
	while (current && current->type == NODE_UNIT) {
		check_statement(s, current->expr.unit_node.expr);
		current = current->expr.unit_node.next;
	}

	pop_scope(s);
}

static void analyze_unit(sema *s, ast_node *node)
{
	ast_node *current = node;
	while (current && current->type == NODE_UNIT) {
		order_type(s, current->expr.unit_node.expr);
		current = current->expr.unit_node.next;
	}

	create_types(s);

	current = node;
	while (current && current->type == NODE_UNIT) {
		if (current->expr.unit_node.expr->type == NODE_FUNCTION) {
			create_prototype(s, current->expr.unit_node.expr);
		}
		current = current->expr.unit_node.next;
	}

	current = node;
	while (current && current->type == NODE_UNIT) {
		if (current->expr.unit_node.expr->type == NODE_FUNCTION) {
			check_function(s, current->expr.unit_node.expr);
		}
		current = current->expr.unit_node.next;
	}
}

sema *sema_init(parser *p, arena *a)
{
	sema *s = arena_alloc(a, sizeof(sema));
	s->allocator = a;
	types = NULL;
	s->ast = p->ast;

	global_scope = arena_alloc(a, sizeof(scope));
	global_scope->parent = NULL;
	global_scope->defs = NULL;
	current_scope = global_scope;

	register_type(s, "void", create_integer(s, "void", 0, false));
	register_type(s, "bool", create_integer(s, "bool", 8, false));
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


