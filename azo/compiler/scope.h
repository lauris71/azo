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

#include <azo/node.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The data about compiler variable during resolve/compile phase
 * 
 * A variable can refer to two different cases:
 *  - A stack (local) variable (parent == NULL) - a local variable defined either in current scope or in parent scope,
 *    can be both LValue and RValue. The position field marks frame-relative stack address.
 *  - A data variable (parent != NULL) - a "closured" variable from parent frame, only RValue. The position field
 *    marks the index in CompiledFunction data array. Parent field points to the variable in parent frame.
 * 
 * Stack and data variables are kept in separate lists - stack variables in scope, data variables in frame
 */
struct _AZOVariable {
	AZOVariable *next;
	AZString *name;
	/* Stack or data position */
	unsigned int pos;
	/* Link to parent for data variables */
	AZOVariable *parent;

	/* Value if determined to be const */
	AZONode *const_expr;
};

AZOVariable *azo_variable_new_stack(AZString *name, AZOVariable *next, unsigned int pos);
AZOVariable *azo_variable_new_data(AZString *name, AZOVariable *next, unsigned int pos, AZOVariable *parent);
void azo_variable_delete (AZOVariable *var);

struct _AZOScope {
	AZOScope *parent;
	/* First free stack position */
	unsigned int next_var_pos;
	AZOVariable *variables;
};

AZOScope *azo_scope_new (AZOScope *parent, unsigned int next_var_pos);
void azo_scope_delete (AZOScope *scope);

unsigned int azo_scope_get_size (AZOScope *scope);

static inline AZOVariable *
azo_scope_lookup_local_var (AZOScope *scope, AZString *name)
{
	for (AZOVariable *var = scope->variables; var; var = var->next) if (var->name == name) return var;
	return NULL;
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

/*
 * Ensures that scope has a local copy of the variable so the const assignment (var = CONST) does not propagate
 * the constant expression into the parent scope.
 * If the variable is already local, returns it.
 *
 * The variable has to be stack-based (data variables are read-only anyways)
 */
AZOVariable *azo_scope_ensure_local_var(AZOScope *scope, AZOVariable *var);

#ifdef __cplusplus
}
#endif

#endif
