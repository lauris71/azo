#ifndef __AZO_COMPILER_SCOPE_H__
#define __AZO_COMPILER_SCOPE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021-2026
 */

typedef struct _AZOScope AZOScope;

#include <az/reference.h>

#include <azo/node.h>
#include <azo/compiler/variable.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOScope {
	AZOScope *parent;
	/* First free stack position */
	unsigned int next_var_pos;
	AZOVariableList *variables;
};

AZOScope *azo_scope_new (AZOScope *parent, unsigned int next_var_pos);
void azo_scope_delete (AZOScope *scope);

unsigned int azo_scope_get_size (AZOScope *scope);

static inline AZOVariable *
azo_scope_lookup_local_var (AZOScope *scope, AZString *name)
{
	AZOVariableList *list = azo_var_list_find(scope->variables, name);
	return (list) ? &list->var : NULL;
}

static inline AZOVariable *
azo_scope_lookup_local_var_chained (AZOScope *scope, AZString *name)
{
	while (scope) {
		AZOVariable *var = azo_scope_lookup_local_var (scope, name);
		if (var) return var;
		scope = scope->parent;
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
