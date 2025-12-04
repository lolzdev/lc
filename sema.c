#define STB_DS_IMPLEMENTATION
#include "sema.h"
#include <string.h>

static struct { char *key; type *value; } *types;

static char *intern_string(sema *s, char *str, usize len)
{
	char *ptr = arena_alloc(s->allocator, len + 1);
	memcpy(ptr, str, len);
	ptr[len] = '\0';
	return ptr;
}

static type *analyze_type(sema *s, ast_node *node)
{
	if (node->type == NODE_STRUCT || node->type == NODE_UNION) {
		type *t = arena_alloc(s->allocator, sizeof(type));
		t->tag = node->type == NODE_STRUCT ? TYPE_STRUCT : TYPE_UNION;
		t->data.structure.name = node->expr.structure.name;
		t->data.structure.name_len = node->expr.structure.name_len;
		t->data.structure.members = node->expr.structure.members;
		shput(types, intern_string(s, t->data.structure.name, t->data.structure.name_len), t);
		return t;
	}

	if (node->type == NODE_ENUM) {
		type *t = arena_alloc(s->allocator, sizeof(type));
		t->tag = TYPE_ENUM;

		t->data.enm.name = node->expr.enm.name;
		t->data.enm.name_len = node->expr.enm.name_len;
		t->data.enm.variants = node->expr.enm.variants;
		shput(types, intern_string(s, t->data.enm.name, t->data.enm.name_len), t);
		return t;
	}

	return NULL;
}

static void analyze_unit(sema *s, ast_node *node)
{
	ast_node *current = node;
	while (current && current->type == NODE_UNIT) {
		if (analyze_type(s, current)) goto end;
end:
		current = current->expr.unit_node.next;
	}
}

sema *sema_init(parser *p, arena *a)
{
	sema *s = arena_alloc(a, sizeof(sema));
	s->allocator = a;
	types = NULL;
	s->ast = p->ast;

	analyze_unit(s, s->ast);

	return s;
}


