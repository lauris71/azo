#ifndef __AZO_PROGRAM_H__
#define __AZO_PROGRAM_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

typedef struct _AZOProgram AZOProgram;
typedef struct _AZOProgramClass AZOProgramClass;

#define AZO_TYPE_PROGRAM azo_program_get_type()

#include <az/value.h>

#include <azo/datablock.h>
#include <azo/code.h>
#include <azo/context.h>
#include <azo/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOProgramClass {
	AZReferenceClass ref_class;
};

struct _AZOProgram {
	AZReference reference;
	AZOContext *ctx;

	unsigned int n_args;
	unsigned int ret_type;
	unsigned int has_this;
	unsigned int n_captures;
	unsigned int n_static;
	unsigned int n_const;
	unsigned int n_shared;
	/* Typecode */
	unsigned char *tcode;
	unsigned int tcode_length;
	/* Immediate values */
	AZODataBlock shared_data;
	/* Debug info */
	AZODebugInfo debug;
};

unsigned int azo_program_get_type();

static inline void
azo_program_ref(AZOProgram *prog) {
	az_reference_ref((AZReference *) prog);
}

static inline void
azo_program_unref(AZOProgram *prog) {
	az_reference_unref((AZReferenceClass *) AZ_IMPL_FROM_TYPE(AZO_TYPE_PROGRAM), (AZReference *) prog);
}

/**
 * @brief Create new program
 * 
 * Creates a new program and transfers bytecode and debug data from code
 * 
 * @param code a compiled AZOCode object
 * @return a new AZOProgram
 */
AZOProgram *azo_program_new(AZOContext *ctx, AZOFrame *frame, AZONode *tree, AZOSource *src);

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

#ifdef __cplusplus
}
#endif

#endif
