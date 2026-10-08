#define __AZO_PROGRAM_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <stdlib.h>
#include <assert.h>

#include <az/extend.h>

#include <azo/bytecode.h>
#include <azo/debugger.h>
#include <azo/parser.h>
#include <azo/keyword.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>
#include <azo/compiler/resolver.h>

#include <azo/program.h>

static void azo_program_finalize (AZOProgramClass *klass, AZOProgram *program);

unsigned int
azo_program_get_type()
{
	static unsigned int type = 0;
	unsigned int t = AZ_TYPE_READ(type);
	if (t) return t;
	AZ_TYPES_LOCK();
	if (!type) {
		az_register_type(&type, (const unsigned char *) "AZOProgram", AZ_TYPE_REFERENCE, sizeof (AZOProgramClass), sizeof (AZOProgram),
			AZ_FLAG_FINAL | AZ_FLAG_ZERO_MEMORY, 0, 0,
			NULL,
			NULL,
			(void (*) (const AZImplementation *, void *)) azo_program_finalize);
	}
	t = type;
	AZ_TYPES_UNLOCK();
	return t;
}

static void
azo_program_finalize (AZOProgramClass *klass, AZOProgram *program)
{
	if (program->tcode) free (program->tcode);
	azo_datablock_finalize(&program->shared_data);
}

AZOProgram *
azo_program_new(AZOContext *ctx, AZOFrame *frame, AZONode *tree, AZOSource *src)
{
	AZOProgram *prog = (AZOProgram *) az_instance_new(AZO_TYPE_PROGRAM);
	AZOCode *code = &frame->code;

	prog->ctx = ctx;
	prog->tcode = code->bc;
	prog->tcode_length = code->bc_len;

	prog->n_args = frame->n_args;
	prog->ret_type = frame->ret_type;
	prog->n_captures = frame->n_captures;
	prog->n_static = frame->n_static;
	prog->n_const = code->data_len;
	prog->n_shared = frame->n_shared;

	/* Initialize static datablock */
	azo_datablock_init(&prog->shared_data, prog->n_const, prog->n_const + prog->n_shared);
	/* Write constants to static block */
	for (unsigned int i = 0; i < code->data_len; i++) {
		azo_datablock_transfer_val(&prog->shared_data, i, code->data[i].impl, &code->data[i].v, 0);
	}
	free (code->data);
	code->data = NULL;
	code->data_size = 0;
	if (code->exprs) {
		azo_debug_info_setup(&prog->debug, code, src);
	}
	/* Clear code */
	code->bc = NULL;
	code->bc_len = 0;
	code->bc_size = 0;
	code->data_len = 0;

	return prog;
}

void
azo_program_print_bytecode (AZOProgram *prog)
{
	unsigned int ic = 0;
	while (ic < prog->tcode_length) {
		uint8_t c[1024];
		azo_bc_print_instruction(c, 1024, prog->tcode, ic, prog->tcode_length);
		fprintf(stdout, "%04d %s\n", ic, c);
		ic = azo_bc_next_instruction(prog->tcode, ic, prog->tcode_length);
	}
}

AZOProgram *
azo_program_compile_from_text(AZOContext *globals, const uint8_t *name,
	const AZImplementation *this_impl, void *this_inst, unsigned int ret_type, unsigned int n_args, AZString *arg_names[], const unsigned int arg_types[],
	const uint8_t *code, unsigned int code_len)
{
	AZOSource *src = azo_source_new_duplicate(name, code, code_len);
	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZONode *expr = azo_parser_parse (&parser);
	//azo_node_print_info(expr, stderr, src, 0);

	AZOCompilerContext comp_ctx = {
		.ret_type = ret_type
	};
	AZOCompiler comp;
	azo_compiler_setup(&comp, globals, src);
	comp.debug = 1;

	if (!this_impl) {
		comp_ctx.frame = azo_compiler_new_frame(&comp, NULL, 0, n_args, ret_type);
		comp_ctx.this_variant = AZO_COMPILER_NO_THIS;
	} else if (!this_inst) {
		comp_ctx.frame = azo_compiler_new_frame(&comp, NULL, 0, n_args, ret_type);
		/* This is argument 0 */
		AZOVariable *var = azo_frame_declare_this(comp_ctx.frame, AZ_IMPL_TYPE(this_impl));
		comp_ctx.this_variant = AZO_COMPILER_THIS_IS_VARIABLE;
		comp_ctx.this_var_pos = var->pos;
	} else {
		comp_ctx.frame = azo_compiler_new_frame(&comp, NULL, 0, n_args, ret_type);
		comp_ctx.this_variant = AZO_COMPILER_THIS_IS_SHARED;
		comp_ctx.this_static_pos = azo_frame_append(comp_ctx.frame, this_impl, this_inst);
	}
	for (unsigned int i = 0; i < n_args; i++) {
		if (!azo_frame_declare_variable(comp_ctx.frame, arg_names[i], arg_types[i])) {
			fprintf(stderr, "Variable %s is already defined in current scope\n", arg_names[i]->str);
		}
		// fixme: fail
	}
	int result = azo_compiler_resolve_program(&comp, &comp_ctx, expr);
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return NULL;
	}
	//azo_node_print_info(expr, stderr, src, 0);
	AZOOptimizer opt;
	azo_optimizer_setup(&opt, &comp);
	result = azo_compiler_optimize_program(&opt, &comp_ctx, expr, AZO_OPTIMIZER_FLAG_ALL);
	azo_optimizer_release(&opt);
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return NULL;
	}

	AZOProgram *prog = azo_compiler_compile (&comp, &comp_ctx, expr, src);
	azo_parser_release (&parser);
	azo_source_unref(src);
	azo_compiler_release(&comp);
	return prog;
}

void
azo_program_interpret(AZOProgram *prog, AZOInterpreter *intr, AZODataBlock *static_data, unsigned int n_args, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size)
{
	unsigned int prev_frame = azo_interpreter_push_frame (intr, 0);
	//fprintf(stderr, "Starting program %s\n", (prog->debug.src->name) ? prog->debug.src->name->str : (const uint8_t *) "unnamed");
	//fprintf(stderr, "Num frames %u\n", intr->n_frames);
	//azo_datablock_print(&prog->shared_data, stderr);
	azo_intepreter_push_values (intr, arg_impls, arg_vals, n_args);
	if (0 && prog->debug.n_terms) {
		AZODebugger *debugger = azo_debugger_new(intr);
		azo_debugger_run(debugger, prog);
		azo_debugger_unref(debugger);
	} else {
		AZOInterpreterCtx ictx = {
			.tcode = prog->tcode,
			.tcode_len = prog->tcode_length,
			.static_data = static_data,
			.shared_data = &prog->shared_data,
			.debug = &prog->debug
		};
		azo_interpreter_run(intr, &ictx);
	}
	*ret_impl = az_value_transfer_autobox(intr->vals[0].impl, ret_val, &intr->vals[0].v.value, ret_size);
	azo_interpreter_restore_frame (intr, prev_frame);
}
