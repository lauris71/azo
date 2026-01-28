#define __AZO_PROGRAM_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <stdlib.h>

#include <az/packed-value.h>
#include <azo/bytecode.h>
#include <azo/parser.h>
#include <azo/compiler/compiler.h>

#include <azo/program.h>

void
azo_program_delete (AZOProgram *program)
{
	unsigned int i;
	if (program->tcode) free (program->tcode);
	for (i = 0; i < program->nvalues; i++) az_packed_value_clear (&program->values[i]);
	free (program->values);
	free (program);
}

void
azo_program_print_bytecode (AZOProgram *program)
{
	print_bytecode (program);
}

AZOProgram *
azo_program_compile_from_text(AZOContext *ctx,
	const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int n_args, AZString *arg_names[], const unsigned int arg_types[],
	const uint8_t *code, unsigned int code_len)
{
	AZOCompiler comp;
	azo_compiler_init(&comp, ctx);
	azo_compiler_push_frame(&comp, this_impl, this_inst, ret_type);
	for (unsigned int i = 0; i < n_args; i++) {
		azo_compiler_declare_variable (&comp, arg_names[i], arg_types[i]);
	}
	AZOSource *src = azo_source_new_static(code, code_len);
	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZOExpression *expr = azo_parser_parse (&parser);
	AZOProgram *prog = azo_compiler_compile (&comp, expr, src);
	azo_parser_release (&parser);
	azo_source_unref(src);
	azo_compiler_finalize(&comp);
	return prog;
}

void
azo_program_interpret(AZOProgram *prog, AZOInterpreter *intr, const AZImplementation *arg_impls[], const AZValue *arg_vals[], unsigned int n_args, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size)
{
	unsigned int frame = intr->n_frames;
	azo_interpreter_push_frame (intr, 0);
	azo_intepreter_push_values (intr, arg_impls, arg_vals, n_args);
	interpreter_interpret (intr, prog, ret_impl, ret_val, ret_size);
	azo_interpreter_restore_frame (intr, frame);
}
