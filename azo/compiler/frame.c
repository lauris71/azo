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

AZOVariable *
azo_compiler_var_new (AZString *name, AZOVariable *next, unsigned int is_val, unsigned int pos)
{
	AZOVariable *var = (AZOVariable *) malloc (sizeof (AZOVariable));
	memset (var, 0, sizeof (AZOVariable));
	var->next = next;
	var->name = name;
	az_string_ref (var->name);
	var->is_val = is_val;
	var->pos = pos;
	return var;
}

void
azo_compiler_var_delete (AZOVariable *var)
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
		azo_compiler_var_delete (var);
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
azo_scope_lookup (AZOScope *scope, AZString *name)
{
	AZOVariable *var;
	for (var = scope->variables; var; var = var->next) if (var->name == name) return var;
	return NULL;
}

AZOVariable *
azo_scope_lookup_chained (AZOScope *scope, AZString *name)
{
	while (scope) {
		AZOVariable *var = azo_scope_lookup (scope, name);
		if (var) return var;
		scope = scope->parent;
	}
	return NULL;
}

AZOVariable *
azo_scope_ensure_local_var (AZOScope *scope, AZOVariable *var)
{
	AZOVariable *loc;
	for (loc = scope->variables; loc; loc = loc->next) if (loc == var) return var;
	loc = azo_compiler_var_new (var->name, scope->variables, var->is_val, var->pos);
	loc->parent_is_val = var->parent_is_val;
	loc->parent_pos = var->parent_pos;
	loc->const_expr = var->const_expr;
	return loc;
}

AZOFrame *
azo_frame_new (AZOFrame *parent, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type)
{
	AZOFrame *frame = (AZOFrame *) malloc (sizeof (AZOFrame));
	memset (frame, 0, sizeof (AZOFrame));
	frame->parent = parent;
	frame->ret_type = ret_type;
	frame->this_impl = this_impl;
	frame->this_inst = this_inst;
	/* fixme: Declare this like other variables? */
	frame->scope = azo_scope_new (NULL, 1);
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
		azo_compiler_var_delete (var);
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
azo_frame_lookup_var (AZOFrame *frame, AZString *name)
{
	return azo_scope_lookup_chained (frame->scope, name);
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
		AZOVariable *var = azo_frame_lookup_var (frame, name);
		if (var) return var;
		var = azo_frame_lookup_parent_var (frame, name);
		if (var) return var;
		frame = frame->parent;
	}
	return NULL;
}

void
azo_frame_write_ic (AZOFrame *frame, unsigned int ic)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(&frame->code, &ic8, 1);
}

void
azo_frame_write_ic_u8 (AZOFrame *frame, unsigned int ic, unsigned int val)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val};
	azo_code_write_bc(&frame->code, &ic8, 2);
}

void
azo_frame_write_ic_u32 (AZOFrame *frame, unsigned int ic, unsigned int val)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(&frame->code, &ic8, 1);
	azo_code_write_bc(&frame->code, &val, 4);
}

void
azo_frame_write_ic_u8_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val1};
	azo_code_write_bc(&frame->code, &ic8, 2);
	azo_code_write_bc(&frame->code, &val2, 4);
}

void
azo_frame_write_ic_u32_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(&frame->code, &ic8, 1);
	azo_code_write_bc(&frame->code, &val1, 4);
	azo_code_write_bc(&frame->code, &val2, 4);
}

void
azo_frame_write_ic_type_value (AZOFrame *frame, unsigned int ic, unsigned int type, const AZValue *val)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(&frame->code, &ic8, 1);
	uint8_t t8 = type;
	azo_code_write_bc(&frame->code, &t8, 1);
	if (type) {
		unsigned int val_size = az_class_value_size(AZ_CLASS_FROM_TYPE(type));
		if (val_size) {
			azo_code_write_bc(&frame->code, val, val_size);
		}
	}
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
	if (azo_scope_lookup (frame->scope, name)) {
		*result = AZO_FRAME_VARIABLE_DEFINED;
		return NULL;
	}
	frame->scope->variables = azo_compiler_var_new (name, frame->scope->variables, 0, frame->scope->next_var_pos++);
	*result = AZO_FRAME_NO_ERROR;
	return frame->scope->variables;
}

AZOVariable *
azo_frame_ensure_variable (AZOFrame *frame, AZString *name)
{
	AZOVariable *var = azo_frame_lookup_var (frame, name);
	if (var) return var;
	var = azo_frame_lookup_parent_var (frame, name);
	if (var) return var;
	if (frame->parent) {
		AZOVariable *prev = azo_frame_ensure_variable (frame->parent, name);
		if (!prev) return NULL;
		var = azo_compiler_var_new (name, frame->parent_vars, 1, frame->n_parent_vars++);
		var->parent_is_val = prev->is_val;
		var->parent_pos = prev->pos;
		var->const_expr = prev->const_expr;
		frame->parent_vars = var;
		return var;
	}
	return NULL;
}
