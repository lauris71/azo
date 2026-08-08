#define __AZO_COMPILER_FRAME_C__

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

#include <azo/compiler/frame.h>

AZOFrame *
azo_frame_new (AZOFrame *parent, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int debug)
{
	AZOFrame *frame = (AZOFrame *) malloc (sizeof (AZOFrame));
	memset (frame, 0, sizeof (AZOFrame));
	frame->parent = parent;
	frame->ret_type = ret_type;
	frame->this_impl = this_impl;
	frame->this_inst = this_inst;
	/* fixme: Declare this like other variables? */
	frame->scope = azo_scope_new (NULL, 1);
	azo_code_init(&frame->code, debug);
	return frame;
}

void
azo_frame_delete (AZOFrame *frame)
{
	while (frame->scope) {
		azo_frame_pop_scope (frame);
	}
	azo_code_clear(&frame->code);
	while (frame->parent_vars) {
		AZOVariable *var = frame->parent_vars;
		frame->parent_vars = var->next;
		azo_variable_delete (var);
	}
	free (frame);
}

void
azo_frame_delete_tree (AZOFrame *frame)
{
	if (frame->parent) azo_frame_delete_tree(frame->parent);
	azo_frame_delete (frame);
}

void
azo_frame_push_scope (AZOFrame *frame)
{
	frame->scope = azo_scope_new (frame->scope, frame->scope->next_var_pos);
}

void
azo_frame_pop_scope (AZOFrame *frame)
{
	AZOScope *scope = frame->scope;
	frame->scope = scope->parent;
	azo_scope_delete (scope);
}

AZOVariable *
azo_frame_lookup_local_var (AZOFrame *frame, AZString *name)
{
	return azo_scope_lookup_local_var_chained (frame->scope, name);
}

AZOVariable *
azo_frame_lookup_parent_var (AZOFrame *frame, AZString *name)
{
	AZOVariable *var;
	for (var = frame->parent_vars; var; var = var->next) if (var->name == name) return var;
	return NULL;
}

AZOVariable *
azo_frame_lookup_chained (AZOFrame *frame, AZString *name)
{
	while (frame) {
		AZOVariable *var = azo_frame_lookup_local_var (frame, name);
		if (var) return var;
		var = azo_frame_lookup_parent_var (frame, name);
		if (var) return var;
		frame = frame->parent;
	}
	return NULL;
}

unsigned int
azo_frame_get_current_ip (AZOFrame *frame)
{
	return frame->code.bc_len;
}

void
azo_frame_update_JMP_to (AZOFrame *frame, unsigned int loc)
{
	int32_t raddr = (int) frame->code.bc_len - (int) (loc + 5);
	memcpy(frame->code.bc + loc + 1, &raddr, 4);
}

void
azo_frame_reserve_data (AZOFrame *frame, unsigned int amount)
{
	azo_code_reserve_data(&frame->code, amount);
}

#define noDEBUG_APPEND

unsigned int
azo_frame_append_value (AZOFrame *frame, unsigned int type, const AZValue *val)
{
	unsigned int pos = frame->code.data_len;
	azo_code_write_instance(&frame->code, AZ_IMPL_FROM_TYPE(type), az_value_get_inst(AZ_IMPL_FROM_TYPE(type), val));
	return pos;
}

unsigned int
azo_frame_append_string (AZOFrame *frame, AZString *str)
{
	int pos = azo_code_find_block(&frame->code, (const AZImplementation *) &AZStringKlass, str);
	if (pos >= 0) return pos;
	return azo_code_write_instance(&frame->code, (const AZImplementation *) &AZStringKlass, str);
}

unsigned int
azo_frame_append_object (AZOFrame *frame, AZObject *obj)
{
	int pos = azo_code_find_block(&frame->code, (const AZImplementation *) obj->klass, obj);
	if (pos >= 0) return pos;
	return azo_code_write_instance(&frame->code, (const AZImplementation *) obj->klass, obj);
}

AZOVariable *
azo_frame_declare_variable (AZOFrame *frame, AZString *name, unsigned int type, unsigned int *result)
{
	if (azo_scope_lookup_local_var (frame->scope, name)) {
		*result = AZO_FRAME_VARIABLE_DEFINED;
		return NULL;
	}
	frame->scope->variables = azo_variable_new_stack(name, frame->scope->variables, frame->scope->next_var_pos++);
	*result = AZO_FRAME_NO_ERROR;
	return frame->scope->variables;
}

AZOVariable *
azo_frame_ensure_variable (AZOFrame *frame, AZString *name)
{
	AZOVariable *var = azo_frame_lookup_local_var (frame, name);
	if (var) return var;
	var = azo_frame_lookup_parent_var (frame, name);
	if (var) return var;
	if (frame->parent) {
		AZOVariable *prev = azo_frame_ensure_variable (frame->parent, name);
		if (!prev) return NULL;
		var = azo_variable_new_data(name, frame->parent_vars, frame->n_parent_vars++, prev);
		frame->parent_vars = var;
		return var;
	}
	return NULL;
}
