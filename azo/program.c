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

AZOProgram *
azo_program_new(AZOContext *ctx, AZOCode *code, AZOExpression *tree, AZOSource *src)
{
	AZOProgram *prog = (AZOProgram *) malloc(sizeof(AZOProgram));
	memset (prog, 0, sizeof (AZOProgram));
	prog->ctx = ctx;
	prog->tcode = code->bc;
	prog->tcode_length = code->bc_len;
	prog->values = code->data;
	prog->nvalues = code->data_len;
	if (code->exprs) {
		azo_debug_info_setup(&prog->debug, code, src);
	}
	/* Clear code */
	code->bc = NULL;
	code->bc_len = 0;
	code->bc_size = 0;
	code->data = NULL;
	code->data_size = 0;
	code->data_len = 0;

	return prog;
}

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
	AZOProgram *prog = azo_compiler_compile (&comp, expr, 1, src);
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
	azo_interpreter_run (intr, prog);
	*ret_impl = az_value_transfer_autobox(intr->vals[0].impl, ret_val, &intr->vals[0].v.value, ret_size);
	azo_interpreter_restore_frame (intr, frame);
}
