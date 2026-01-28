#ifndef __AZO_COMPILER_FRAME_H__
#define __AZO_COMPILER_FRAME_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021
 */

typedef struct _AZOExpression AZOExpression;

typedef struct _AZOVariable AZOVariable;
typedef struct _AZOScope AZOScope;
typedef struct _AZOFrame AZOFrame;

#include <az/packed-value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOVariable {
	AZOVariable *next;
	AZString *name;
	/* Variable data */
	/* Whether pos refers to stack (0) or data (1) */
	unsigned int is_val : 1;
	unsigned int pos : 31;
	/* If a readonly copy from parent frame then parent's data */
	/* Whether parent_pos refers to stack (0) or data (1) */
	unsigned int parent_is_val : 1;
	unsigned int parent_pos : 31;

	/* Value if determined to be const */
	AZOExpression *const_expr;
};

struct _AZOScope {
	AZOScope *parent;
	/* First free stack position */
	unsigned int next_var_pos;
	AZOVariable *variables;
};

AZOScope *azo_scope_new (AZOScope *parent, unsigned int next_var_pos);
void azo_scope_delete (AZOScope *scope);
unsigned int azo_scope_get_size (AZOScope *scope);
AZOVariable *azo_scope_lookup (AZOScope *scope, AZString *name);
AZOVariable *azo_scope_lookup_chained (AZOScope *scope, AZString *name);
/* Ensures that scope has local copy of variable so const assignment does not propagate to parent scope */
AZOVariable *azo_scope_ensure_local_var (AZOScope *scope, AZOVariable *var);

#define AZO_FRAME_NO_ERROR 0
#define AZO_FRAME_VARIABLE_DEFINED 1
#define AZO_FRAME_VARIABLE_NOT_DEFINED 1

/**
 * @brief The compilation context
 * 
 * A container for independent bytecode object (program or function)
 * AZOFrame manages scopes and implements closure, const data and bytecode storage
 */
struct _AZOFrame {
	/**
	 * @brief Link to the parent frame (containing function)
	 * 
	 */
	AZOFrame *parent;
	/**
	 * @brief Return type of this code block
	 * 
	 * NULL for void block.
	 */
	unsigned int ret_type;
	/* NULL for static, Any for relocatable code */
	const AZImplementation *this_impl;
	const AZValue *this_val;
	/* Current scope */
	AZOScope *scope;
	/* Parent variables */
	unsigned int n_parent_vars;
	AZOVariable *parent_vars;
	/* Compiled bytecode */
	unsigned int bc_len;
	unsigned int bc_size;
	unsigned char *bc;
	/* Data */
	/* fixme: Implement as stack/array */
	unsigned int data_size;
	unsigned int data_len;
	AZPackedValue *data;
};

AZOFrame *azo_frame_new (AZOFrame *parent, const AZImplementation *this_impl, const AZValue *this_val, unsigned int ret_type);
void azo_frame_delete (AZOFrame *frame);
void azo_frame_delete_tree (AZOFrame *frame);

void azo_frame_push_scope (AZOFrame *frame);
void azo_frame_pop_scope (AZOFrame *frame);

AZOVariable *azo_frame_lookup_var (AZOFrame *frame, AZString *name);
AZOVariable *azo_frame_lookup_parent_var (AZOFrame *frame, AZString *name);
AZOVariable *azo_frame_lookup_chained (AZOFrame *frame, AZString *name);

void azo_frame_ensure_bc_size (AZOFrame *frame, unsigned int amount);
void azo_frame_ensure_data_size (AZOFrame *frame, unsigned int amount);
void azo_frame_write_ic (AZOFrame *frame, unsigned int ic);
void azo_frame_write_ic_u8 (AZOFrame *frame, unsigned int ic, unsigned int val);
void azo_frame_write_ic_u32 (AZOFrame *frame, unsigned int ic, unsigned int val);
void azo_frame_write_ic_u8_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2);
void azo_frame_write_ic_u32_u32 (AZOFrame *frame, unsigned int ic, unsigned int val1, unsigned int val2);
void azo_frame_write_ic_type_value (AZOFrame *frame, unsigned int ic, unsigned int type, const AZValue *val);

unsigned int azo_frame_get_current_ip (AZOFrame *frame);
void azo_frame_update_JMP_to (AZOFrame *frame, unsigned int loc);

void azo_frame_reserve_data (AZOFrame *frame, unsigned int amount);
unsigned int azo_frame_append_value (AZOFrame *frame, unsigned int type, const AZValue *val);
unsigned int azo_frame_append_string (AZOFrame *frame, AZString *str);
unsigned int azo_frame_append_object (AZOFrame *frame, AZObject *obj);

/* Declares variable in current scope */
AZOVariable *azo_frame_declare_variable (AZOFrame *frame, AZString *name, unsigned int type, unsigned int *result);
AZOVariable *azo_frame_ensure_variable (AZOFrame *frame, AZString *name);

#ifdef __cplusplus
}
#endif

#endif
