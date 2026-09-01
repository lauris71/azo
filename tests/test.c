#define __TEST_C__

#include <string.h>

#include <az/az.h>
#include <az/classes/value-array-ref.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/program.h>

#include "unity/unity.h"

static void test_compile();
static void test_assign();
static void test_function();

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
        } else if (!strcmp(argv[i], "function")) {
            RUN_TEST(test_function);
        }
    }
    return UNITY_END();
}

static const char *compile_src = ""
"int32 a = 1;\n"
"int32 b = 2;\n"
"int32 c = (int32) (a + b);\n"
"return c;\n"
"";

static void
test_compile()
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) compile_src, strlen(compile_src));
	AZOProgram *prog = azo_program_compile_from_text(ctx, (const uint8_t *) "test-program", NULL, NULL,
        AZ_TYPE_INT32, 0, NULL, NULL,
        (const uint8_t *) compile_src, strlen(compile_src));
    azo_program_print_bytecode(prog);
    const AZImplementation *this_impl = NULL;
    const AZValue *this_val = NULL;
    AZPackedValue ret_val;
	azo_program_interpret(prog, ctx->intr, 0, NULL, NULL, &ret_val.impl, &ret_val.v, AZ_PACKED_VALUE_MAX_SIZE);
    TEST_ASSERT_EQUAL_INT(3, ret_val.v.int32_v);

    az_packed_value_clear(&ret_val);
    azo_program_delete(prog);
    az_object_unref((AZObject *) src);
    azo_context_delete(ctx);
}

static const char *assign_src = ""
"int32 a = 1;\n"
"a += 1;\n"
"return a;\n"
"";

static void
test_assign()
{
    az_init();
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) assign_src, strlen(assign_src));
	AZOProgram *prog = azo_program_compile_from_text(ctx, (const uint8_t *) "test-program", NULL, NULL,
        AZ_TYPE_INT32, 0, NULL, NULL,
        (const uint8_t *) assign_src, strlen(assign_src));
    azo_program_print_bytecode(prog);
    const AZImplementation *this_impl = NULL;
    const AZValue *this_val = NULL;
    AZPackedValue ret_val;
	azo_program_interpret(prog, ctx->intr, 0, NULL, NULL, &ret_val.impl, &ret_val.v, AZ_PACKED_VALUE_MAX_SIZE);
    TEST_ASSERT_EQUAL_INT(2, ret_val.v.int32_v);

    az_packed_value_clear(&ret_val);
    azo_program_delete(prog);
    az_object_unref((AZObject *) src);
    azo_context_delete(ctx);
}

static const char *function_src = ""
"any a = function int32 (int32 a, int32 b) {\n"
"    for (int32 i = 0; i < b; i++) a = a + 1;\n"
"    return a;\n"
"};\n"
"int32 c = a(100, 28);\n"
"return c;\n"
"";

static void
test_function()
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
