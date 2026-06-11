#ifndef __AZO_COMPILER_SCOPE_H__
#define __AZO_COMPILER_SCOPE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021-2026
 */

typedef struct _AZOVariable AZOVariable;
typedef struct _AZOScope AZOScope;

#include <az/reference.h>

#include <azo/expression.h>

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
	unsigned int parent_pos;

	/* Value if determined to be const */
	AZOExpression *const_expr;
};

AZOVariable *azo_variable_new_stack(AZString *name, AZOVariable *next, unsigned int pos);
AZOVariable *azo_variable_new_data(AZString *name, AZOVariable *next, unsigned int pos);
void azo_compiler_var_delete (AZOVariable *var);

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
/*
 * Ensures that scope has a local copy of the variable so the const assignment does not propagate to the parent scope.
 * If the variable is already local, returns it.
 *
 * The variable has to be stack-based (data variables are read-only anyways)
 */
AZOVariable *azo_scope_ensure_local_var(AZOScope *scope, AZOVariable *var);

#ifdef __cplusplus
}
#endif

#endif
