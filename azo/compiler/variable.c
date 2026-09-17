#define __AZO_COMPILER_VARIABLE_C__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021-2026
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <az/object.h>
#include <az/packed-value.h>
#include <az/string.h>

#include <azo/compiler/scope.h>

void
azo_variable_init(AZOVariable *var, AZString *name, unsigned int pos)
{
    memset (var, 0, sizeof (AZOVariable));
    var->name = name;
    az_string_ref(name);
    var->pos = pos;
}

void
azo_variable_finalize(AZOVariable *var)
{
    az_string_unref (var->name);
}

void
azo_var_list_free_one(AZOVariableList *list)
{
	azo_variable_finalize(&list->var);
	free(list);
}

void
azo_var_list_free(AZOVariableList *list)
{
	while (list) {
		AZOVariableList *next = list->next;
		azo_var_list_free_one(list);
		list = next;
	}
}

AZOVariableList *
azo_var_list_find(AZOVariableList *list, AZString *name)
{
	assert(name != NULL);
	while (list) {
		if (list->var.name == name) return list;
		list = list->next;
	}
	return NULL;
}

/* Unconditional prepend */
AZOVariableList *
azo_var_list_prepend(AZOVariableList *list, AZString *name, unsigned int pos)
{
	assert(name != NULL);
	AZOVariableList *node = calloc(1, sizeof(AZOVariableList));
	azo_variable_init(&node->var, name, pos);
	node->next = list;
	return node;
}

AZOVariableList *
azo_var_list_set(AZOVariableList *list, AZString *name, unsigned int pos, const AZONode *const_node)
{
	assert(name != NULL);
	AZOVariableList *node = azo_var_list_find(list, name);
	if (node) {
		node->var.pos = pos;
		node->var.const_node = const_node;
	} else {
		list = azo_var_list_prepend(list, name, pos);
		list->var.const_node = const_node;
	}
	return list;
}

AZOVariableList *
azo_var_list_remove(AZOVariableList *list, AZString *name)
{
	assert(name != NULL);
	AZOVariableList *current = list;
	AZOVariableList *prev = NULL;
	while (current) {
		if (current->var.name == name) {
			if (prev) {
				prev->next = current->next;
			} else {
				list = current->next;
			}
			azo_var_list_free_one(current);
			return list;
		}
		prev = current;
		current = current->next;
	}
	return list;
}

AZOVariableList *
azo_var_list_remove_all(AZOVariableList *list, AZOVariableList *other)
{
	for (AZOVariableList *v = other; v; v = v->next) {
		list = azo_var_list_remove(list, v->var.name);
	}
	return list;
}

AZOVariableList *
azo_var_list_duplicate(AZOVariableList *list)
{
	AZOVariableList *result = NULL;
	for (AZOVariableList *v = list; v; v = v->next) {
		assert(v->var.name != NULL);
		result = azo_var_list_prepend(result, v->var.name, v->var.pos);
		result->var.const_node = v->var.const_node;
	}
	return result;
}

AZOVariableList *
azo_var_list_reverse(AZOVariableList *list)
{
    AZOVariableList *result = NULL;
    while (list) {
        AZOVariableList *next = list->next;
        list->next = result;
        result = list;
        list = next;
    }
    return result;
}
