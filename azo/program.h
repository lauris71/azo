#ifndef __AZO_PROGRAM_H__
#define __AZO_PROGRAM_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <az/value.h>

#include <azo/code.h>
#include <azo/context.h>
#include <azo/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOProgram AZOProgram;

struct _AZOProgram {
	AZOContext *ctx;
	/* Typecode */
	unsigned char *tcode;
	unsigned int tcode_length;
	/* Immediate values */
	unsigned int nvalues;
	AZPackedValue *values;
	/* Debug info */
	AZODebugInfo debug;
};

/**
 * @brief Create new program
 * 
 * Creates a new program and transfers bytecode and debug data from code
 * 
 * @param code a compiled AZOCode object
 * @return a new AZOProgram
 */
AZOProgram *azo_program_new(AZOContext *ctx, AZOCode *code, AZONode *tree, AZOSource *src);

void azo_program_delete (AZOProgram *program);

void azo_program_print_bytecode (AZOProgram *program);

AZOProgram *azo_program_compile_from_text(AZOContext *ctx, const uint8_t *name,
	const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int n_args, AZString *arg_names[], const unsigned int arg_types[],
	const uint8_t *code, unsigned int code_len);

/**
 * @brief Interpret a program
 * 
 * @param prog The program to interpret
 * @param intr The interpreter instance
 * @param n_args The number of arguments (including this)
 * @param arg_impls Array of argument implementations (including this)
 * @param arg_vals Array of argument values (including this)
 * @param ret_impl Pointer to store the return implementation
 * @param ret_val Pointer to store the return value
 * @param ret_size Size of the return value buffer
 */
void azo_program_interpret(AZOProgram *prog, AZOInterpreter *intr, unsigned int n_args, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size);
void azo_program_interpret_call(AZOProgram *prog, AZOInterpreter *intr, const AZImplementation *arg_impls[], const AZValue *arg_vals[], unsigned int n_args, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size);

#ifdef __cplusplus
}
#endif

#endif
