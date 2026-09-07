#ifndef __AZO_COMPILER_FRAME_H__
#define __AZO_COMPILER_FRAME_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2021
 */

typedef struct _AZONode AZONode;

typedef struct _AZOFrame AZOFrame;

#include <az/packed-value.h>

#include <azo/code.h>
#include <azo/compiler/scope.h>

#ifdef __cplusplus
extern "C" {
#endif

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
	/**
	 * @brief Reference to this
	 * 
	 * It is up to invoker to ensure that this stays valid.
	 * Implementation is NULL for static code, instance is NULL for relocatable code.
	 */
	const AZImplementation *this_impl;
	void *this_inst;
	/* Current scope */
	AZOScope *scope;
	/*
	 * Parent variables
	 *
	 * These are read-only references to variables from parent frames.
	 * During the compilation of function body these are reserved to program data values.
	 * The actual values are filled by binding during the execution of the code that defines the function.
	 */
	unsigned int n_parent_vars;
	AZOVariable *parent_vars;
	/* Compiled bytecode */
	AZOCode code;
};

/**
 * @brief Create a new frame
 * 
 * @param parent The parent frame (NULL for root)
 * @param this_impl Implementation of the current object (NULL for static code)
 * @param this_inst Instance of the current object or NULL if not known at compile time
 * @param ret_type Return type of the code block (AZ_TYPE_NONE for void)
 * @param debug Debug flag (non-zero to enable debugging)
 * @return Pointer to the newly created frame, or NULL on failure
 */
AZOFrame *azo_frame_new (AZOFrame *parent, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int debug);
void azo_frame_delete (AZOFrame *frame);
void azo_frame_delete_tree (AZOFrame *frame);

void azo_frame_push_scope (AZOFrame *frame);
void azo_frame_pop_scope (AZOFrame *frame);

AZOVariable *azo_frame_lookup_local_var (AZOFrame *frame, AZString *name);
AZOVariable *azo_frame_lookup_parent_var (AZOFrame *frame, AZString *name);
AZOVariable *azo_frame_lookup_chained (AZOFrame *frame, AZString *name);

unsigned int azo_frame_get_current_ip (AZOFrame *frame);
void azo_frame_update_JMP_to (AZOFrame *frame, unsigned int loc);

void azo_frame_reserve_data (AZOFrame *frame, unsigned int amount);
unsigned int azo_frame_append_value (AZOFrame *frame, unsigned int type, const AZValue *val);
unsigned int azo_frame_append_string (AZOFrame *frame, AZString *str);
unsigned int azo_frame_append_object (AZOFrame *frame, AZObject *obj);

/**
 * @brief Declare variable in current scope
 * 
 * @param frame The frame
 * @param name The variable name
 * @param type The type of variable
 * @param result pointer to error code
 * @return The variable object
 *
 */
AZOVariable *azo_frame_declare_variable (AZOFrame *frame, AZString *name, unsigned int type, unsigned int *result);
/**
 * @brief Ensure variable exists in current frame
 * 
 * Searches both local and parent variables for given name. If not found, recursively calls
 * azo_frame_ensure_variable in parent frame and if found, creates a parent variable in active
 * frame refering to it.
 * 
 * This is used to force variables from parent frames to be available in current frame for
 * binding to closures (compiled fonctions).
 * 
 * @param frame The frame
 * @param name The variable name
 * @return The existing or created variable object or NULL, is there is no such name in the chain.
 * 
 */
AZOVariable *azo_frame_ensure_variable (AZOFrame *frame, AZString *name);

#ifdef __cplusplus
}
#endif

#endif
