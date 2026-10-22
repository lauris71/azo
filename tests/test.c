#define __TEST_C__

#include <string.h>

#include <az/az.h>
#include <az/classes/value-array.h>
#include <az/io/buffer-output-stream.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/program.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>

#include "unity/unity.h"
#include "test.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

int
main(int argc, const char *argv[])
{
    UNITY_BEGIN();
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "tokenizer")) {
            RUN_TEST(test_tokenizer);
        } else if (!strcmp(argv[i], "parser")) {
            RUN_TEST(test_parser);
        } else if (!strcmp(argv[i], "resolver")) {
            RUN_TEST(test_resolver);
        } else if (!strcmp(argv[i], "compile")) {
            RUN_TEST(test_compile);
        } else if (!strcmp(argv[i], "language")) {
            RUN_TEST(test_language);
        } else if (!strcmp(argv[i], "assign")) {
            RUN_TEST(test_assign);
        } else if (!strcmp(argv[i], "comparison")) {
            RUN_TEST(test_comparison);
        } else if (!strcmp(argv[i], "function")) {
            RUN_TEST(test_function);
        } else if (!strcmp(argv[i], "cycles")) {
            RUN_TEST(test_cycles);
        } else if (!strcmp(argv[i], "calculate")) {
            RUN_TEST(test_calculate);
        } else if (!strcmp(argv[i], "calculate_binary")) {
            RUN_TEST(test_calculate_binary);
        } else {
            fprintf(stderr, "Unknown test: %s\n", argv[i]);
        }
    }
    return UNITY_END();
}

static AZODataBlock static_data = {0};

static int
test_program_noargs(AZOContext *ctx, const char *text, const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val)
{
    az_init();
    AZOContext *globals = azo_context_new();
    azo_context_define_basic_types(globals);

    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) text, strlen(text));

	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZONode *tree = azo_parser_parse(&parser);
	azo_node_print_info(tree, stderr, src, 0);

	AZOCompilerContext comp_ctx = {
		.ret_type = AZ_TYPE_INT32,
		.exception_mask = AZO_COMPILER_DEFAULT_EXCEPTION_MASK,
		.calc_mode = AZO_CALCULATE_MODE_SATURATE
	};
	AZOCompiler comp;
	azo_compiler_setup(&comp, globals, src);
	comp.debug = 1;

	AZOFrame *frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_INT32);

    comp_ctx.frame = frame;
	int result = azo_compiler_resolve_program(&comp, &comp_ctx, tree);
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
		.ret_type = AZ_TYPE_INT32,
		.exception_mask = AZO_COMPILER_DEFAULT_EXCEPTION_MASK,
		.calc_mode = AZO_CALCULATE_MODE_SATURATE
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
		.ret_type = AZ_TYPE_INT32,
		.exception_mask = AZO_COMPILER_DEFAULT_EXCEPTION_MASK,
		.calc_mode = AZO_CALCULATE_MODE_SATURATE
	};
	comp_ctx.frame = frame;
	AZOProgram *prog = azo_compiler_compile (&comp, &comp_ctx, tree, src);

    if (!prog) return 1;
    azo_program_print_bytecode(prog);
    azo_interpreter_init(ctx->intr);
	azo_program_interpret(prog, ctx->intr, &static_data, 0, NULL, NULL, ret_impl, ret_val, AZ_VALUE_MAX_SIZE);
    azo_program_unref(prog);
    az_object_unref((AZObject *) src);
    return 0;
}

static const char *binary_arithmetic_i32_src = ""
"int32 a = 1;\n"
"int32 b = 2;\n"
"int32 c = (int32) (a + b);\n"
"return c;\n"
"";
static const char *_compile_src = ""
"return 3;\n"
"";

void
test_compile(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    const AZImplementation *ret_impl;
    AZValue ret_val;
    TEST_ASSERT(test_program_noargs(ctx, binary_arithmetic_i32_src, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(3, ret_val.int32_v);
    azo_context_delete(ctx);
}

/* Various language features */
void
test_language(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZBufferOutputStream *bostream = (AZBufferOutputStream *) az_instance_new(AZ_TYPE_BUFFER_OUTPUT_STREAM);

    const AZImplementation *arg_impls[16];
	const AZValue *arg_vals[16];
    AZValue vals[16];
    arg_impls[0] = AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM);
    az_value_set_from_inst(AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM), &vals[0], bostream);
    arg_vals[0] = &vals[0];
	const char *arg_names[] = {"ofs"};

	/* Deferred block is executed at the end of the scope */
    {
        const char *src =
        "ofs.print(\"S1 \");\n"
        "defer { ofs.print(\"S2 \"); };\n"
        "ofs.print(\"S3 \");\n"
        "return 42;\n";
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(ctx, src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("S1 S3 S2 ", str);
		free(str);
    }
	/* Nested deferred blocks */
    {
        const char *src =
        "ofs.print(\"S1 \");\n"
        "defer {\n"
        "  ofs.print(\"S2 \");\n"
        "  defer { ofs.print(\"S3 \"); };\n"
        "  ofs.print(\"S4 \");\n"
        "}\n"
        "ofs.print(\"S5 \");\n"
        "return 42;\n";
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(ctx, src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("S1 S5 S2 S4 S3 ", str);
		free(str);
    }
    azo_context_delete(ctx);
}

static const char *assign_src = ""
"int32 a = 1;\n"
"a += 1;\n"
"return a;\n"
"";

void
test_assign(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    const AZImplementation *ret_impl;
    AZValue ret_val;
    TEST_ASSERT(test_program_noargs(ctx, assign_src, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(2, ret_val.int32_v);
    azo_context_delete(ctx);
}

void
test_cycles(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    const AZImplementation *ret_impl;
    AZValue ret_val;
    /* Empty for */
    TEST_ASSERT(test_program_noargs(ctx, "int32 i;for (i = 0; i < 10; i++); return i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(10, ret_val.int32_v);
    /* For with double update */
    TEST_ASSERT(test_program_noargs(ctx, "int32 i, j = 0;for (i = 0; i < 10; i++) {i += 1;j += 1;} return j;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(5, ret_val.int32_v);
    /* Empty init and step */
    TEST_ASSERT(test_program_noargs(ctx, "int32 i = 0;for (; i < 10;) i += 1; return i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(10, ret_val.int32_v);
    /* Variable resolution inside */
    TEST_ASSERT(test_program_noargs(ctx, "int32 r; for (int32 i = 0, k = 10; i < k; i += 2, k += 1) r = i; return r;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(18, ret_val.int32_v);
    /* Simple while */
    TEST_ASSERT(test_program_noargs(ctx, "int32 i = 0; while (i < 20) {i += 1;}\nreturn i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(20, ret_val.int32_v);
    /* Simple do */
    TEST_ASSERT(test_program_noargs(ctx, "int32 i = 1; do {i *= 7;} while (i < 1000000);\nreturn i;\n", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(5764801, ret_val.int32_v);
    azo_context_delete(ctx);
}

/* Compile and run a program, returning the int32 result */

int32_t
run_program(const char *source)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZOProgram *prog = azo_program_compile_from_text(ctx, (const uint8_t *) "test-program", NULL, NULL,
        AZ_TYPE_INT32, 0, NULL, NULL,
        (const uint8_t *) source, strlen(source));
    AZPackedValue ret_val;
    azo_interpreter_init(ctx->intr);
    azo_program_interpret(prog, ctx->intr, &static_data, 0, NULL, NULL, &ret_val.impl, &ret_val.v, AZ_PACKED_VALUE_MAX_SIZE);
    int32_t result = ret_val.v.int32_v;
    az_packed_value_clear(&ret_val);
    azo_program_unref(prog);
    azo_context_delete(ctx);
    return result;
}

/* Comparison operators return correct results (the parser no longer normalizes
 * GE/GT to LE/LT by swapping operands - the compiler handles all four) */

void
test_comparison(void)
{
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 2 > 1;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 r = 1 > 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 2 >= 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 r = 1 >= 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 1 < 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 r = 2 < 1;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 2 <= 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 r = 2 <= 1;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 2 == 2;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 r = 2 != 1;\nreturn r;\n"));
    /* non-constant operands */
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 a = 5;\nint32 b = 3;\nint32 r = a > b;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 a = 5;\nint32 b = 3;\nint32 r = a < b;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(1, run_program("int32 a = 5;\nint32 b = 5;\nint32 r = a >= b;\nreturn r;\n"));
    TEST_ASSERT_EQUAL_INT(0, run_program("int32 a = 5;\nint32 b = 6;\nint32 r = a >= b;\nreturn r;\n"));
}

