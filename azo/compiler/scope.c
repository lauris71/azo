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
	azo_var_list_free (scope->variables);
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
