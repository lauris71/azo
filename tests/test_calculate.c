#define __AZO_TEST_CALCULATE_C__

#include <az/io/buffer-output-stream.h>
#include <az/value.h>

#include <azo/context.h>
#include <azo/datablock.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>

#include "unity/unity.h"
#include "test.h"

static const char *binary_arithmetic_src = ""
"int32 a = 31;\n"
"int32 b = 3;\n"
"string space = \" \";\n"
"ofs.print(a + b);\n"
"ofs.print(space);\n"
"ofs.print(a - b);\n"
"ofs.print(space);\n"
"ofs.print(a / b);\n"
"ofs.print(space);\n"
"ofs.print(a * b);\n"
"ofs.print(space);\n"
"ofs.print(a << b);\n"
"ofs.print(space);\n"
"ofs.print(a >> b);\n"
"ofs.print(space);\n"
"ofs.print(a & b);\n"
"ofs.print(space);\n"
"ofs.print(a | b);\n"
"ofs.print(space);\n"
"ofs.print(a ^ b);\n"
"return 42;\n"
"";

static const char *binary_comparison_src = ""
"int32 a = 31;\n"
"int32 b = 3;\n"
"string space = \" \";\n"
"ofs.print(a == b);\n"
"ofs.print(space);\n"
"ofs.print(a != b);\n"
"ofs.print(space);\n"
"ofs.print(a < b);\n"
"ofs.print(space);\n"
"ofs.print(a <= b);\n"
"ofs.print(space);\n"
"ofs.print(a > b);\n"
"ofs.print(space);\n"
"ofs.print(a >= b);\n"
"return 42;\n"
"";

static AZODataBlock static_data = {0};
static AZOContext *globals = NULL;
static AZBufferOutputStream *bostream = NULL;

static int
test_program(AZOContext *ctx, const char *text, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val)
{
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) text, strlen(text));

	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZONode *tree = azo_parser_parse(&parser);
	azo_node_print_info(tree, stderr, src, 0);

	AZOCompilerContext comp_ctx = {
		.ret_type = AZ_TYPE_INT32
	};
	AZOCompiler comp;
	azo_compiler_setup(&comp, globals, src);
	comp.debug = 1;

	AZOFrame *frame = azo_compiler_push_frame(&comp, NULL, NULL, 1, AZ_TYPE_INT32);
	unsigned int dr;
	AZString *ofs = az_string_new((const uint8_t *) "ofs");
    azo_frame_declare_variable(frame, ofs, AZ_TYPE_OUTPUT_STREAM, &dr);

    comp_ctx.frame = frame;
	int result = azo_compiler_resolve_program(&comp, &comp_ctx, tree, NULL, NULL);
	azo_node_print_info(tree, stderr, src, 0);
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return 1;
	}
	azo_node_print_info(tree, stderr, src, 0);
	AZOOptimizer opt;
	azo_optimizer_setup(&opt, &comp);
	comp_ctx = (AZOCompilerContext) {
		.ret_type = AZ_TYPE_INT32
	};
	comp_ctx.frame = frame;
	result = azo_compiler_optimize_program(&opt, &comp_ctx, tree, AZO_OPTIMIZER_FLAG_ALL);
	azo_optimizer_release(&opt);
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return 1;
	}

	comp_ctx = (AZOCompilerContext) {
		.ret_type = AZ_TYPE_INT32
	};
	comp_ctx.frame = frame;
	AZOProgram *prog = azo_compiler_compile (&comp, &comp_ctx, tree, src);

    if (!prog) return 1;
    azo_program_print_bytecode(prog);
	azo_program_interpret(prog, ctx->intr, &static_data, 1, arg_impls, arg_vals, ret_impl, ret_val, AZ_VALUE_MAX_SIZE);
    azo_program_unref(prog);
    az_object_unref((AZObject *) src);
    return 0;
}

void
test_calculate(void)
{
    az_init();
    globals = azo_context_new();
    azo_context_define_basic_types(globals);
    bostream = (AZBufferOutputStream *) az_instance_new(AZ_TYPE_BUFFER_OUTPUT_STREAM);

    const AZImplementation *arg_impls[16];
	const AZValue *arg_vals[16];
    AZValue vals[16];
    arg_impls[0] = AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM);
    az_value_set_from_inst(AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM), &vals[0], bostream);
    arg_vals[0] = &vals[0];

	/* Binary arithmetic operations */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_arithmetic_src, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("34 28 10 93 248 3 3 31 28", str);
		free(str);
    }
	/* Binary comparison operations */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_comparison_src, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("false true false false true true", str);
		free(str);
    }
	azo_context_delete(globals);
}

