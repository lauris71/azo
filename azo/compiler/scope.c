#define __AZO_COMPILER_SCOPE_C__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <az/object.h>
#include <az/packed-value.h>
#include <az/string.h>

#include <azo/compiler/scope.h>

AZOVariable *
azo_variable_new_stack (AZString *name, AZOVariable *next, unsigned int pos)
{
	AZOVariable *var = (AZOVariable *) malloc (sizeof (AZOVariable));
	memset (var, 0, sizeof (AZOVariable));
	var->next = next;
	var->name = name;
	az_string_ref (var->name);
	var->pos = pos;
	return var;
}

AZOVariable *
azo_variable_new_data (AZString *name, AZOVariable *next, unsigned int pos, AZOVariable *parent)
{
	AZOVariable *var = (AZOVariable *) malloc (sizeof (AZOVariable));
	memset (var, 0, sizeof (AZOVariable));
	var->next = next;
	var->name = name;
	az_string_ref (var->name);
	var->pos = pos;
    var->parent = parent;
    var->const_expr = parent->const_expr;
	return var;
}

void
azo_variable_delete (AZOVariable *var)
{
	az_string_unref (var->name);
	free (var);
}

AZOScope *
azo_scope_new (AZOScope *parent, unsigned int next_var_pos)
{
	AZOScope *scope = ( AZOScope *) malloc (sizeof (AZOScope));
	memset (scope, 0, sizeof (AZOScope));
	scope->parent = parent;
	scope->next_var_pos = next_var_pos;
	return scope;
}

void
azo_scope_delete (AZOScope *scope)
{
	while (scope->variables) {
		AZOVariable *var = scope->variables;
		scope->variables = var->next;
		azo_variable_delete (var);
	}
	free (scope);
}

unsigned int
azo_scope_get_size (AZOScope *scope)
{
	if (scope->parent) {
		return scope->next_var_pos - scope->parent->next_var_pos;
	} else {
		return scope->next_var_pos;
	}
}

AZOVariable *
azo_scope_ensure_local_var(AZOScope *scope, AZOVariable *var)
{
	AZOVariable *loc;
	for (loc = scope->variables; loc; loc = loc->next) {
        if (loc == var) return var;
    }
   	loc = azo_variable_new_stack(var->name, scope->variables, var->pos);
	loc->const_expr = var->const_expr;
	return loc;
}
