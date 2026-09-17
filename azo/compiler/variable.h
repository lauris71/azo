#ifndef __AZO_COMPILER_VARIABLE_H__
#define __AZO_COMPILER_VARIABLE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021-2026
 */

typedef struct _AZOVariable AZOVariable;
typedef struct _AZOVariableList AZOVariableList;

#include <az/string.h>

#include <azo/node.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The data about compiler variable during resolve/optimize/compile phase
 * 
 * A variable can refer to two different cases:
 *  - A stack (local) variable (parent == NULL) - a local variable defined either in current scope or in parent scope,
 *    can be both LValue and RValue. The position field marks frame-relative stack address.
 *  - A data variable (parent != NULL) - a "closured" variable from the parent frame, only RValue. The position field
 *    marks the index in CompiledFunction data array. Parent field points to the variable in parent frame.
 * 
 * Stack and data variables are kept in separate lists - stack variables in scope, data variables in frame
 */
struct _AZOVariable {
	AZString *name;
	/* Stack or data position */
	unsigned int pos;
	/* Link to parent for data variables (for resolver) */
	AZOVariable *parent;
    /* Link to const node if determined to be constand (for optimizer) */
	const AZONode *const_node;
};

void azo_variable_init(AZOVariable *var, AZString *name, unsigned int pos);
void azo_variable_finalize(AZOVariable *var);

struct _AZOVariableList {
	AZOVariableList *next;
	AZOVariable var;
};

/**
 * @brief Free one list element ignoring links
 * 
 * @param list The list element to free
 */
void azo_var_list_free_one(AZOVariableList *list);

/**
 * @brief Free the full variable list
 * 
 * @param list The list to free
 */
void azo_var_list_free(AZOVariableList *list);

/**
 * @brief Search named element in a list
 * 
 * @param list The list to search
 * @param name The name to look for
 * @return Pointer to the found element or NULL
 */
AZOVariableList *azo_var_list_find(AZOVariableList *list, AZString *name);
/**
 * @brief Unconditionally prepends a new element to a list
 * 
 * @param list The list to prepend to
 * @param name The name of the variable
 * @param pos The position of the variable
 * @return The new element at the head of the list
 */
AZOVariableList *azo_var_list_prepend(AZOVariableList *list, AZString *name, unsigned int pos);

/**
 * @brief Add or update element in a list
 * 
 * @param list The list to modify
 * @param name The name of the variable
 * @param pos The position of the variable
 * @param const_node The const node if determined to be const
 * @return Updated list with the element added or updated
 */
AZOVariableList *azo_var_list_set(AZOVariableList *list, AZString *name, unsigned int pos, const AZONode *const_node);

/**
 * @brief Remove one element from a list
 * 
 * @param list The list to modify
 * @param name The name of the variable to remove
 * @return Updated list with the element removed
 */
AZOVariableList *azo_var_list_remove(AZOVariableList *list, AZString *name);
/**
 * @brief Remove all elements of one list from the other
 * 
 * @param list The list to modify
 * @param other The list containing elements to remove from list
 * @return Updated list with the elements removed
 */
AZOVariableList *azo_var_list_remove_all(AZOVariableList *list, AZOVariableList *other);
/**
 * @brief Duplicates a list
 * 
 * @param list The list to duplicate
 * @return New duplicated list
 */
AZOVariableList *azo_var_list_duplicate(AZOVariableList *list);

/**
 * @brief Reverse the order of elements in a list
 * 
 * @param list The list to reverse
 * @return The reversed list
 */
AZOVariableList *azo_var_list_reverse(AZOVariableList *list);

#ifdef __cplusplus
}
#endif

#endif
