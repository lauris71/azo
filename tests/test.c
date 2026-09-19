#define __TEST_C__

#include <string.h>

#include <az/az.h>
#include <az/classes/value-array.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/program.h>

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
        if (!strcmp(argv[i], "compile")) {
            RUN_TEST(test_compile);
        } else if (!strcmp(argv[i], "assign")) {
            RUN_TEST(test_assign);
        } else if (!strcmp(argv[i], "comparison")) {
            RUN_TEST(test_comparison);
        } else if (!strcmp(argv[i], "function")) {
            RUN_TEST(test_function);
#ifdef HAS_FUNCTION_KEYWORD
            RUN_TEST(test_legacy_function);
#endif
        } else if (!strcmp(argv[i], "tokenizer")) {
            RUN_TEST(test_tokenizer);
        } else if (!strcmp(argv[i], "parser")) {
            RUN_TEST(test_parser);
        } else if (!strcmp(argv[i], "cycles")) {
            RUN_TEST(test_cycles);
        }
    }
    return UNITY_END();
}

static int
test_program(AZOContext *ctx, const char *text, const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val)
{
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) text, strlen(text));
	AZOProgram *prog = azo_program_compile_from_text(ctx, (const uint8_t *) text, NULL, NULL,
        ret_type, 0, NULL, NULL, src->cdata, src->csize);
    if (!prog) return 1;
	azo_program_interpret(prog, ctx->intr, 0, NULL, NULL, ret_impl, ret_val, AZ_VALUE_MAX_SIZE);
    azo_program_delete(prog);
    az_object_unref((AZObject *) src);
    return 0;
}

static const char *compile_src = ""
"int32 a = 1;\n"
"int32 b = 2;\n"
"int32 c = (int32) (a + b);\n"
"return c;\n"
"";

void
test_compile(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    const AZImplementation *ret_impl;
    AZValue ret_val;
    TEST_ASSERT(test_program(ctx, compile_src, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(3, ret_val.int32_v);
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
    TEST_ASSERT(test_program(ctx, assign_src, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
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
    TEST_ASSERT(test_program(ctx, "int32 i;for (i = 0; i < 10; i++); return i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(10, ret_val.int32_v);
    /* For with double update */
    TEST_ASSERT(test_program(ctx, "int32 i, j = 0;for (i = 0; i < 10; i++) {i += 1;j += 1;} return j;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(5, ret_val.int32_v);
    /* Empty init and step */
    TEST_ASSERT(test_program(ctx, "int32 i = 0;for (; i < 10;) i += 1; return i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(10, ret_val.int32_v);
    /* Variable resolution inside */
    TEST_ASSERT(test_program(ctx, "int32 r; for (int32 i = 0, k = 10; i < k; i += 2, k += 1) r = i; return r;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(18, ret_val.int32_v);
    /* Simple while */
    TEST_ASSERT(test_program(ctx, "int32 i = 0; while (i < 20) {i += 1;}\nreturn i;", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
    TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
    TEST_ASSERT_EQUAL_INT(20, ret_val.int32_v);
    /* Simple do */
    TEST_ASSERT(test_program(ctx, "int32 i = 1; do {i *= 7;} while (i < 1000000);\nreturn i;\n", AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
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
    azo_program_interpret(prog, ctx->intr, 0, NULL, NULL, &ret_val.impl, &ret_val.v, AZ_PACKED_VALUE_MAX_SIZE);
    int32_t result = ret_val.v.int32_v;
    az_packed_value_clear(&ret_val);
    azo_program_delete(prog);
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

static const char *function_src = ""
"any a = (int32 a, int32 b) int32 => {\n"
"    for (int32 i = 0; i < b; i++) a = a + 1;\n"
"    return a;\n"
"};\n"
"int32 c = a(100, 28);\n"
"return c;\n"
"";

void
test_function(void)
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) function_src, strlen(function_src));
	AZOProgram *prog = azo_program_compile_from_text(ctx, (const uint8_t *) "test-program", NULL, NULL,
        AZ_TYPE_INT32, 0, NULL, NULL,
        (const uint8_t *) function_src, strlen(function_src));
    azo_program_print_bytecode(prog);
    const AZImplementation *this_impl = NULL;
    const AZValue *this_val = NULL;
    AZPackedValue ret_val;
	azo_program_interpret(prog, ctx->intr, 0, NULL, NULL, &ret_val.impl, &ret_val.v, AZ_PACKED_VALUE_MAX_SIZE);
    TEST_ASSERT_EQUAL_INT(128, ret_val.v.int32_v);

    az_packed_value_clear(&ret_val);
    azo_program_delete(prog);
    az_object_unref((AZObject *) src);
    azo_context_delete(ctx);
}

#ifdef HAS_FUNCTION_KEYWORD
/* LEGACY - the old function keyword syntax (superseded by lambdas) */
static const char *legacy_function_src = ""
"any a = function int32 (int32 a, int32 b) {\n"
"    for (int32 i = 0; i < b; i++) a = a + 1;\n"
"    return a;\n"
"};\n"
"int32 c = a(100, 28);\n"
"return c;\n"
"";

void
test_legacy_function(void)
{
    TEST_ASSERT_EQUAL_INT(128, run_program(legacy_function_src));
}
#endif
