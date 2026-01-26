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

#include <azo/frame.h>

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
azo_frame_new (AZOFrame *parent, const AZImplementation *this_impl, const AZValue *this_val, unsigned int ret_type)
{
	AZOFrame *frame = (AZOFrame *) malloc (sizeof (AZOFrame));
	memset (frame, 0, sizeof (AZOFrame));
	frame->parent = parent;
	frame->ret_type = ret_type;
	frame->this_impl = this_impl;
	frame->this_val = this_val;
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
	if (frame->bc) free (frame->bc);
	if (frame->data) {
		unsigned int i;
		for (i = 0; i < frame->data_len; i++) az_packed_value_clear (&frame->data[i]);
		free (frame->data);
	}
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
	while (frame->children) {
		AZOFrame *child = frame->children;
		frame->children = child->next;
		azo_frame_delete (child);
	}
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
azo_frame_ensure_bc_size (AZOFrame *frame, unsigned int amount)
{
	if ((frame->bc_len + amount) > frame->bc_size) {
		frame->bc_size <<= 1;
		if (frame->bc_size < (frame->bc_len + amount)) frame->bc_size = frame->bc_len + amount;
		if (frame->bc_size < 256) frame->bc_size = 256;
		frame->bc = (unsigned char *) realloc (frame->bc, frame->bc_size);
	}
}

void
azo_frame_ensure_data_size (AZOFrame *frame, unsigned int amount)
{
	if ((frame->data_len + amount) > frame->data_size) {
		unsigned int new_size;
		new_size = frame->data_size << 1;
		if (new_size < (frame->data_len + amount)) new_size = frame->data_len + amount;
		if (new_size < 16) new_size = 16;
		frame->data = (AZPackedValue *) realloc (frame->data, new_size * sizeof (AZPackedValue));
		memset (&frame->data[frame->data_size], 0, (new_size - frame->data_size) * sizeof (AZPackedValue));
		frame->data_size = new_size;
	}
}

void
azo_frame_write_ic (AZOFrame *frame, unsigned int ic)
{
	azo_frame_ensure_bc_size (frame, 1);
	frame->bc[frame->bc_len++] = (unsigned char) ic;
}

void
azo_frame_write_ic_u8 (AZOFrame *frame, unsigned int ic, unsigned int val)
{
	azo_frame_ensure_bc_size (frame, 2);
	frame->bc[frame->bc_len++] = (unsigned char) ic;
	frame->bc[frame->bc_len++] = (unsigned char) val;
}

void
azo_frame_write_ic_u32 (AZOFrame *frame, unsigned int ic, unsigned int val)
{
	azo_frame_ensure_bc_size (frame, 5);
	frame->bc[frame->bc_len] = (unsigned char) ic;
	memcpy (&frame->bc[frame->bc_len + 1], &val, 4);
	frame->bc_len += 5;
}

void
azo_frame_write_ic_u8_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2)
{
	azo_frame_ensure_bc_size (frame, 6);
	frame->bc[frame->bc_len] = (unsigned char) ic;
	frame->bc[frame->bc_len + 1] = (unsigned char) val1;
	memcpy (&frame->bc[frame->bc_len + 2], &val2, 4);
	frame->bc_len += 6;
}

void
azo_frame_write_ic_u32_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2)
{
	azo_frame_ensure_bc_size (frame, 9);
	frame->bc[frame->bc_len] = (unsigned char) ic;
	memcpy (&frame->bc[frame->bc_len + 1], &val1, 4);
	memcpy (&frame->bc[frame->bc_len + 5], &val2, 4);
	frame->bc_len += 9;
}

void
azo_frame_write_ic_type_value (AZOFrame *frame, unsigned int ic, unsigned int type, const AZValue *val)
{
	azo_frame_ensure_bc_size (frame, 2);
	frame->bc[frame->bc_len++] = (unsigned char) ic;
	frame->bc[frame->bc_len++] = (unsigned char) type;
	if (type) {
		AZClass *klass = az_type_get_class (type);
		if (az_class_value_size(klass)) {
			azo_frame_ensure_bc_size (frame, az_class_value_size(klass));
			memcpy (&frame->bc[frame->bc_len], val, az_class_value_size(klass));
			frame->bc_len += az_class_value_size(klass);
		}
	}
}

unsigned int
azo_frame_get_current_ip (AZOFrame *frame)
{
	return frame->bc_len;
}

void
azo_frame_update_JMP_to (AZOFrame *frame, unsigned int loc)
{
	int32_t raddr = (int) frame->bc_len - (int) (loc + 5);
	memcpy (frame->bc + loc + 1, &raddr, 4);
}

void
azo_frame_reserve_data (AZOFrame *frame, unsigned int amount)
{
	assert (frame->data_len == 0);
	azo_frame_ensure_data_size (frame, amount);
	frame->data_len = amount;
}

#define noDEBUG_APPEND

unsigned int
azo_frame_append_value (AZOFrame *frame, unsigned int type, const AZValue *val)
{
	azo_frame_ensure_data_size (frame, 1);
	unsigned int pos = frame->data_len;
	az_packed_value_set_from_type_value (&frame->data[frame->data_len++], type, val);
	return pos;
}

unsigned int
azo_frame_append_string (AZOFrame *frame, AZString *str)
{
	unsigned int pos;
	for (pos = 0; pos < frame->data_len; pos++) {
		if (frame->data[pos].impl && (frame->data[pos].impl == (const AZImplementation *) az_type_get_class (AZ_TYPE_STRING)) && (frame->data[pos].v.string == str)) return pos;
	}
	azo_frame_ensure_data_size (frame, 1);
	az_packed_value_set_string (&frame->data[frame->data_len++], str);
#ifdef DEBUG_APPEND
	fprintf (stderr, "azo_frame_append_string: Stored %s at %u\n", str->str, pos);
#endif
	return pos;
}

unsigned int
azo_frame_append_object (AZOFrame *frame, AZObject *obj)
{
	unsigned int pos;
	for (pos = 0; pos < frame->data_len; pos++) {
		if (frame->data[pos].impl && (frame->data[pos].impl == (const AZImplementation *) obj->klass) && (frame->data[pos].v.reference == (AZReference *) obj)) return pos;
	}
	azo_frame_ensure_data_size (frame, 1);
	az_packed_value_set_object (&frame->data[frame->data_len++], obj);
#ifdef DEBUG_APPEND
	fprintf (stderr, "azo_frame_append_object: Stored %s at %u\n", ((AZClass *) obj->klass)->name, pos);
#endif
	return pos;
}

AZOVariable *
azo_frame_declare_variable (AZOFrame *frame, AZString *name, unsigned int *result)
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
		if (!prev) NULL;
		var = azo_compiler_var_new (name, frame->parent_vars, 1, frame->n_parent_vars++);
		var->parent_is_val = prev->is_val;
		var->parent_pos = prev->pos;
		var->const_expr = prev->const_expr;
		frame->parent_vars = var;
		return var;
	}
	return NULL;
}
