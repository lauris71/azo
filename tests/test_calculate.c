#define __AZO_TEST_CALCULATE_C__

#include <az/io/buffer-output-stream.h>
#include <az/value.h>

#include <azo/context.h>
#include <azo/datablock.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>

#include "unity/unity.h"
#include "test.h"

static unsigned int print_tree = 0;
static unsigned int print_code = 0;

static const char *binary_arithmetic_i32_src = ""
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
"ofs.print(a % b);\n"
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

static const char *binary_arithmetic_typed_src = ""
"string space = \" \";\n"
"ofs.print(a + b);\n"
"ofs.print(space);\n"
"ofs.print(a - b);\n"
"ofs.print(space);\n"
"ofs.print(a / b);\n"
"ofs.print(space);\n"
"ofs.print(a * b);\n"
"ofs.print(space);\n"
"ofs.print(a % b);\n"
"return 42;\n"
"";

static const char *binary_comparison_src = ""
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

static const char *increment_decrement_src = ""
"int32 a = 31;\n"
"string space = \" \";\n"
"ofs.print(a++);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"ofs.print(a++);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"ofs.print(a--);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"ofs.print(a--);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"return 42;\n"
"";

static AZODataBlock static_data = {0};
static AZOContext *globals = NULL;
static AZBufferOutputStream *bostream = NULL;

int
test_program(AZOContext *ctx, const char *text,
	unsigned int n_args, const char *arg_names[], const AZImplementation *arg_impls[], const AZValue *arg_vals[],
	const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val)
{
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) text, strlen(text));

	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZONode *tree = azo_parser_parse(&parser);
	if (print_tree) azo_node_print_info(tree, stderr, src, 0);

	AZOCompilerContext comp_ctx = {
		.ret_type = AZ_TYPE_INT32,
		.exception_mask = AZO_COMPILER_DEFAULT_EXCEPTION_MASK,
		.calc_mode = AZO_CALCULATE_MODE_SATURATE
	};
	AZOCompiler comp;
	azo_compiler_setup(&comp, ctx, src);
	comp.debug = 1;

	AZOFrame *frame = azo_compiler_new_frame(&comp, NULL, 0, 1, ret_type);
	for (unsigned int i = 0; i < n_args; ++i) {
		AZString *str = az_string_new((const uint8_t *) arg_names[i]);
		azo_frame_declare_variable(frame, str, AZ_IMPL_TYPE(arg_impls[i]), 0);
	}

    comp_ctx.frame = frame;
	int result = azo_compiler_resolve_program(&comp, &comp_ctx, tree);
	if (print_tree) azo_node_print_info(tree, stderr, src, 0);
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return 1;
	}
	if (print_tree) azo_node_print_info(tree, stderr, src, 0);
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
    if (print_code) azo_program_print_bytecode(prog);
    azo_interpreter_init(ctx->intr);
    azo_context_clear_exception(ctx);
	azo_program_interpret(prog, ctx->intr, &static_data, n_args, arg_impls, arg_vals, ret_impl, ret_val, AZ_VALUE_MAX_SIZE);
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
	arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
	vals[1].int32_v = 31;
	arg_vals[1] = &vals[1];
	arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
	vals[2].int32_v = 3;
	arg_vals[2] = &vals[2];
	const char *arg_names[] = {"ofs", "a", "b"};

	/* Binary arithmetic operations int32-int32 */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_arithmetic_i32_src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("34 28 10 93 1 248 3 3 31 28", str);
		free(str);
    }
	/* Binary arithmetic operations int32-float */
    {
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_FLOAT);
        vals[2].float_v = 3.0;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_arithmetic_typed_src, 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("34.000 28.000 10.333 93.000 1.0000", str);
		free(str);
    }
	/* Binary comparison operations with const operands */
    {
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 3;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_comparison_src, 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("false true false false true true", str);
		free(str);
    }
	/* Increment/Decrement postfix operations */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, increment_decrement_src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("31 32 32 33 33 32 32 31", str);
		free(str);
    }
	/* Increment/Decrement prefix operations */
    {
		print_code = 1;
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        const char *src = ""
"int32 a = 31;\n"
"string space = \" \";\n"
"debug interpreter_stack;\n"
"ofs.print(++a);\n"
"debug interpreter_stack;\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"debug interpreter_stack;\n"
"ofs.print(++a);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"ofs.print(--a);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"ofs.print(space);\n"
"ofs.print(--a);\n"
"ofs.print(space);\n"
"ofs.print(a);\n"
"return 42;\n"
"";
        TEST_ASSERT(test_program(globals, src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("32 32 33 33 32 32 31 31", str);
		free(str);
    }
	/* Static variables persist per-closure across calls */
    {
		print_code = 0;
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals,
            "any c = () int32 => { static int32 n = 0; n = n + 1; return n; };\n"
            "return c() + c() + c();\n",
            0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(6, ret_val.int32_v);
    }
	/* Shared variables persist across calls of the same lambda */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals,
            "any c = () int32 => { shared int32 n = 0; n = n + 1; return n; };\n"
            "return c() + c() + c();\n",
            0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(6, ret_val.int32_v);
    }
	/* Shared and static semantics */
    {
		const char *src = 
			"for (int32 i = 0; i < 3; i++) {\n"
            "  function c = () => { static int32 n = 0; shared int32 m = 0; n = n + 1; m = m + 1;\n"
			"    ofs.print(\"m=\");ofs.print(m);ofs.print(\" \");\n"
		    "    ofs.print(\"n=\");ofs.print(n);ofs.print(\" \");\n"
			"  };\n"
			"  c();c();c()\n"
            "};\n";
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, src, 1, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("m=1 n=1 m=2 n=2 m=3 n=3 m=4 n=1 m=5 n=2 m=6 n=3 m=7 n=1 m=8 n=2 m=9 n=3 ", str);
		free(str);
    }
	/* Binary arithmetic operations int32-int32 (runtime values, not pre-computed) */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = 31;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 3;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, binary_arithmetic_typed_src, 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("34 28 10 93 1", str);
		free(str);
    }
	/* Division by zero throws AZO_EXCEPTION_DIVIDE_BY_ZERO */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = 31;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 0;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a / b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_DIVIDE_BY_ZERO, globals->az_ctx.exc.type);
    }
	/* Modulo by zero throws AZO_EXCEPTION_DIVIDE_BY_ZERO */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = 31;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 0;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a % b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_DIVIDE_BY_ZERO, globals->az_ctx.exc.type);
    }
	/* int32 addition overflow throws AZO_EXCEPTION_INTEGER_OVERFLOW */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = INT32_MAX;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 1;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a + b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_INTEGER_OVERFLOW, globals->az_ctx.exc.type);
    }
	/* int64 multiplication overflow throws AZO_EXCEPTION_INTEGER_OVERFLOW */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64);
        vals[1].int64_v = INT64_MAX;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64);
        vals[2].int64_v = 2;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a * b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_INTEGER_OVERFLOW, globals->az_ctx.exc.type);
    }
	/* int64 addition with int32 promotes to int64 without overflow */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64);
        vals[1].int64_v = 3000000000LL;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 1;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a + b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64), ret_impl);
        TEST_ASSERT_EQUAL_INT64(3000000001LL, ret_val.int64_v);
        TEST_ASSERT_EQUAL_UINT32(AZ_EXCEPTION_NONE, globals->az_ctx.exc.type);
    }
	/* Bitwise and shift operations int32 (runtime values) */
    {
        static const char *src = ""
"ofs.print(a << b);ofs.print(\" \");\n"
"ofs.print(a >> b);ofs.print(\" \");\n"
"ofs.print(a & b);ofs.print(\" \");\n"
"ofs.print(a | b);ofs.print(\" \");\n"
"ofs.print(a ^ b);\n"
"return 42;\n";
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = 31;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 3;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
		az_buffer_output_stream_reset(bostream);
        TEST_ASSERT(test_program(globals, src, 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
		char *str = strndup((const char *) bostream->buffer, bostream->pos);
		fprintf(stderr, "Output: %s\n", str);
		TEST_ASSERT_EQUAL_STRING("248 3 3 31 28", str);
		free(str);
    }
	/* int64 shift left keeps int64 result */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64);
        vals[1].int64_v = 8;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[2].int32_v = 2;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a << b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT64), ret_impl);
        TEST_ASSERT_EQUAL_INT64(32, ret_val.int64_v);
        TEST_ASSERT_EQUAL_UINT32(AZ_EXCEPTION_NONE, globals->az_ctx.exc.type);
    }
	/* bitwise operations on float throw AZO_EXCEPTION_INVALID_TYPE */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_FLOAT);
        vals[1].float_v = 3.0;
        arg_vals[1] = &vals[1];
        arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_FLOAT);
        vals[2].float_v = 2.0;
        arg_vals[2] = &vals[2];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return a & b;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZ_EXCEPTION_INVALID_TYPE, globals->az_ctx.exc.type);
    }
	/* Arithmetic assignment is allowed (converted at runtime) */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "int8 x = 42;x = x + 1;return x;\n", 0, arg_names, arg_impls, arg_vals, AZ_TYPE_INT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(43, ret_val.int32_v);
        TEST_ASSERT(test_program(globals, "uint32 a = 1; a = a + 1; return a;\n", 0, NULL, NULL, NULL, AZ_TYPE_UINT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(2, ret_val.uint32_v);
    }
	/* Magnitude-changing implicit assignment throws at runtime */
    {
        arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
        vals[1].int32_v = 31;
        arg_vals[1] = &vals[1];
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* uint8 x = a (31); x = x + 250 -> 281 does not fit uint8 */
        TEST_ASSERT(test_program(globals, "uint8 x = a; x = x + 250; return x;\n", 3, arg_names, arg_impls, arg_vals, AZ_TYPE_UINT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, globals->az_ctx.exc.type);
    }
	/* Explicit cast follows the computation mode */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* Default: narrowing cast throws */
        TEST_ASSERT(test_program(globals, "return (uint8) 300;\n", 0, NULL, NULL, NULL, AZ_TYPE_UINT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, globals->az_ctx.exc.type);
        /* saturate: clamps */
        TEST_ASSERT(test_program(globals, "return saturate((uint8) 300);\n", 0, NULL, NULL, NULL, AZ_TYPE_UINT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(255, ret_val.uint8_v);
        /* wrap: truncates */
        TEST_ASSERT(test_program(globals, "return wrap((uint8) 300);\n", 0, NULL, NULL, NULL, AZ_TYPE_UINT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(44, ret_val.uint8_v);
    }
	/* Exact cast qualifier throws on rounding */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* (int32) 1.5f rounds to 1 (masked by default) */
        TEST_ASSERT(test_program(globals, "return (int32) 1.5f;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(1, ret_val.int32_v);
        /* (int32 exact) 1.5f throws CHANGE_OF_PRECISION */
        TEST_ASSERT(test_program(globals, "return (int32 exact) 1.5f;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_CHANGE_OF_PRECISION, globals->az_ctx.exc.type);
    }
	/* Constant propagation stores the converted (mode-aware) value */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* int32 300 -> int8 clamps to 127 under saturate */
        TEST_ASSERT(test_program(globals, "saturate { int8 b = (int8) 300; return b; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(127, ret_val.int8_v);
        /* int32 300 -> int8 truncates to 44 under wrap */
        TEST_ASSERT(test_program(globals, "wrap { int8 b = (int8) 300; return b; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(44, ret_val.int8_v);
    }
	/* Mode block and mode expression fall back to generic processing */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, "return wrap (2 + 3);\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(5, ret_val.int32_v);
        TEST_ASSERT(test_program(globals, "saturate { return 10 + 5; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(15, ret_val.int32_v);
        TEST_ASSERT(test_program(globals, "int32 v = finite (2 * 4); return v;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(8, ret_val.int32_v);
    }
	/* Computation mode drives constant folding of overflow/non-finite */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* Default: constant integer overflow is an error */
        TEST_ASSERT(test_program(globals, "int32 x = 2147483647 + 1; return x;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 1);
        /* saturate: clamps with a warning */
        TEST_ASSERT(test_program(globals, "saturate { int32 x = 2147483647 + 1; return x; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MAX, ret_val.int32_v);
        /* wrap: wraps with a warning */
        TEST_ASSERT(test_program(globals, "wrap { int32 x = 2147483647 + 1; return x; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MIN, ret_val.int32_v);
	/* finite: constant non-finite result is an error */
        TEST_ASSERT(test_program(globals, "finite { double d = 1.0 / 0.0; return d; }\n", 0, NULL, NULL, NULL, AZ_TYPE_DOUBLE, &ret_impl, &ret_val) == 1);
    }
	/* Unary minus is constant-folded with overflow detection */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* Exact negation */
        TEST_ASSERT(test_program(globals, "int32 v = -7; return v;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(-7, ret_val.int32_v);
        /* Negating INT8_MIN under wrap folds to -128 (wrapped), not an error */
        TEST_ASSERT(test_program(globals, "wrap { int8 v = -(int8) -128; return v; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT8, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(-128, ret_val.int8_v);
    }
	/* Runtime computation mode (non-constant operands) */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* Default: runtime integer overflow throws */
        TEST_ASSERT(test_program(globals, "int32 x = 2147483647; x = x + 1; return x;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 1);
        /* wrap: wraps at runtime */
        TEST_ASSERT(test_program(globals, "wrap { int32 x = 2147483647; x = x + 1; return x; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MIN, ret_val.int32_v);
        /* saturate: clamps at runtime */
        TEST_ASSERT(test_program(globals, "saturate { int32 x = 2147483647; x = x + 1; return x; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MAX, ret_val.int32_v);
        /* finite: runtime non-finite result throws */
        TEST_ASSERT(test_program(globals, "finite { double d = 1.0; double e = 0.0; d = d / e; return d; }\n", 0, NULL, NULL, NULL, AZ_TYPE_DOUBLE, &ret_impl, &ret_val) == 1);
    }
	/* Nested modes and mode expressions with surrounding variables */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        /* Inner saturate overrides outer wrap (variable x declared in outer block) */
        TEST_ASSERT(test_program(globals, "wrap { int32 x = 2147483647; saturate { x = x + 1; } return x; }\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MAX, ret_val.int32_v);
        /* Mode expression with surrounding variables */
        TEST_ASSERT(test_program(globals, "int32 x = 2147483647; int32 y = 1; int32 z = wrap (x + y); return z;\n", 0, NULL, NULL, NULL, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_INT(INT32_MIN, ret_val.int32_v);
    }
	azo_context_delete(globals);
}

