#ifndef __AZO_PROGRAM_H__
#define __AZO_PROGRAM_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <az/value.h>

#include <azo/context.h>

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
};

void azo_program_delete (AZOProgram *program);

void azo_program_print_bytecode (AZOProgram *program);

AZOProgram *azo_program_compile_from_text(AZOContext *ctx,
	const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int n_args, AZString *arg_names[], const unsigned int arg_types[],
	const uint8_t *code, unsigned int code_len);

void azo_program_interpret(AZOProgram *prog, AZOInterpreter *intr, const AZImplementation *arg_impls[], const AZValue *arg_vals[], unsigned int n_args, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size);

#ifdef __cplusplus
}
#endif

#endif
