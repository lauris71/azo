#define __TEST_C__

#include <string.h>

#include <az/az.h>
#include <az/classes/value-array.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/program.h>
#include <azo/tokenizer.h>

#include "unity/unity.h"

static void test_compile();
static void test_assign();
static void test_function();
static void test_tokenizer();

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
        } else if (!strcmp(argv[i], "tokenizer")) {
            RUN_TEST(test_tokenizer);
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

/* Tokenizer tests */

static void
assert_token(const uint8_t *src, unsigned int expected_type, const char *expected_text)
{
    AZOTokenizer tokenizer;
    AZOToken token = {0, 0, AZO_TOKEN_NONE};
    azo_tokenizer_setup(&tokenizer, src, (unsigned int) strlen((const char *) src));
    unsigned int ok = azo_tokenizer_get_next_token(&tokenizer, &token);
    TEST_ASSERT_TRUE_MESSAGE(ok, (const char *) src);
    TEST_ASSERT_EQUAL_HEX_MESSAGE(expected_type, token.type, (const char *) src);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(strlen(expected_text), token.end - token.start, (const char *) src);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected_text, src + token.start, token.end - token.start, (const char *) src);
    azo_tokenizer_release(&tokenizer);
}

static void
assert_invalid(const uint8_t *src)
{
    AZOTokenizer tokenizer;
    AZOToken token = {0, 0, AZO_TOKEN_NONE};
    azo_tokenizer_setup(&tokenizer, src, (unsigned int) strlen((const char *) src));
    unsigned int ok = azo_tokenizer_get_next_token(&tokenizer, &token);
    TEST_ASSERT_FALSE_MESSAGE(ok, (const char *) src);
    TEST_ASSERT_EQUAL_HEX_MESSAGE(AZO_TOKEN_INVALID, token.type, (const char *) src);
    azo_tokenizer_release(&tokenizer);
}

static void
test_tokenizer()
{
    az_init();
    /* Decimal, hex and binary integers */
    assert_token((const uint8_t *) "123", AZO_TOKEN_INTEGER, "123");
    assert_token((const uint8_t *) "0x0", AZO_TOKEN_INTEGER_HEX, "0x0");
    assert_token((const uint8_t *) "0x20", AZO_TOKEN_INTEGER_HEX, "0x20");
    assert_token((const uint8_t *) "0xff00", AZO_TOKEN_INTEGER_HEX, "0xff00");
    assert_token((const uint8_t *) "0x1F", AZO_TOKEN_INTEGER_HEX, "0x1F");
    assert_token((const uint8_t *) "0b101", AZO_TOKEN_INTEGER_BIN, "0b101");
    /* Hex/binary prefix requires at least one digit */
    assert_invalid((const uint8_t *) "0x;");
    assert_invalid((const uint8_t *) "0b;");
    assert_invalid((const uint8_t *) "0x");
    assert_invalid((const uint8_t *) "0b");
    assert_invalid((const uint8_t *) "0xg");
    /* Number directly followed by an operator */
    assert_token((const uint8_t *) "1=2", AZO_TOKEN_INTEGER, "1");
    assert_token((const uint8_t *) "1==2", AZO_TOKEN_INTEGER, "1");
    assert_token((const uint8_t *) "1%2", AZO_TOKEN_INTEGER, "1");
    assert_token((const uint8_t *) "1!=2", AZO_TOKEN_INTEGER, "1");
    assert_token((const uint8_t *) "1?2:3", AZO_TOKEN_INTEGER, "1");
    /* Reals and exponents */
    assert_token((const uint8_t *) "12.5", AZO_TOKEN_FLOATING_POINT, "12.5");
    assert_token((const uint8_t *) "1e5", AZO_TOKEN_FLOATING_POINT, "1e5");
    assert_token((const uint8_t *) "1E5", AZO_TOKEN_FLOATING_POINT, "1E5");
    assert_token((const uint8_t *) "1e-5", AZO_TOKEN_FLOATING_POINT, "1e-5");
    assert_token((const uint8_t *) "1e+5", AZO_TOKEN_FLOATING_POINT, "1e+5");
    assert_token((const uint8_t *) "1.5e3", AZO_TOKEN_FLOATING_POINT, "1.5e3");
    assert_token((const uint8_t *) "1.5e-3", AZO_TOKEN_FLOATING_POINT, "1.5e-3");
    /* Exponent requires at least one digit */
    assert_invalid((const uint8_t *) "1e");
    assert_invalid((const uint8_t *) "1e+");
    assert_invalid((const uint8_t *) "1e-");
    /* Reals starting with the decimal point */
    assert_token((const uint8_t *) ".5", AZO_TOKEN_FLOATING_POINT, ".5");
    assert_token((const uint8_t *) ".5f", AZO_TOKEN_FLOATING_POINT, ".5f");
    assert_token((const uint8_t *) ".5e2", AZO_TOKEN_FLOATING_POINT, ".5e2");
    assert_token((const uint8_t *) ".5e-2", AZO_TOKEN_FLOATING_POINT, ".5e-2");
    /* ...but the dot operator still works */
    assert_token((const uint8_t *) ".", AZO_TOKEN_OPERATOR | AZO_OPERATOR_DOT, ".");
    assert_token((const uint8_t *) "..", AZO_TOKEN_OPERATOR | AZO_OPERATOR_DOT, ".");
    assert_token((const uint8_t *) "a.b", AZO_TOKEN_WORD, "a");
    /* Integer suffixes: u, l, ul, lu (ll is invalid) */
    assert_token((const uint8_t *) "10ul", AZO_TOKEN_INTEGER, "10ul");
    assert_token((const uint8_t *) "1lu", AZO_TOKEN_INTEGER, "1lu");
    assert_token((const uint8_t *) "1UL", AZO_TOKEN_INTEGER, "1UL");
    assert_token((const uint8_t *) "1Lu", AZO_TOKEN_INTEGER, "1Lu");
    assert_invalid((const uint8_t *) "1ll");
    assert_invalid((const uint8_t *) "1ull");
    assert_invalid((const uint8_t *) "1uu");
    assert_invalid((const uint8_t *) "1ulu");
    /* Other number suffixes */
    assert_token((const uint8_t *) "12.5f", AZO_TOKEN_FLOATING_POINT, "12.5f");
    assert_token((const uint8_t *) "3i", AZO_TOKEN_FLOATING_POINT, "3i");
    /* Operators and brackets at the end of input */
    assert_token((const uint8_t *) "=", AZO_TOKEN_OPERATOR | AZO_OPERATOR_ASSIGN, "=");
    assert_token((const uint8_t *) "==", AZO_TOKEN_OPERATOR | AZO_OPERATOR_EQUAL, "==");
    assert_token((const uint8_t *) "+", AZO_TOKEN_OPERATOR | AZO_OPERATOR_PLUS, "+");
    assert_token((const uint8_t *) "<<=", AZO_TOKEN_OPERATOR | AZO_OPERATOR_SHIFT_LEFT_ASSIGN, "<<=");
    assert_token((const uint8_t *) ";", AZO_TOKEN_SEMICOLON, ";");
    assert_token((const uint8_t *) "(", AZO_TOKEN_LEFT_PARENTHESIS, "(");
    /* Comments */
    assert_token((const uint8_t *) "/* c */5", AZO_TOKEN_INTEGER, "5");
    assert_token((const uint8_t *) "// c\n5", AZO_TOKEN_INTEGER, "5");
    /* Words and text */
    assert_token((const uint8_t *) "foo_bar9", AZO_TOKEN_WORD, "foo_bar9");
    assert_token((const uint8_t *) "\"hello \\\" world\"", AZO_TOKEN_TEXT, "\"hello \\\" world\"");
    /* Unterminated C comment consumes the rest of input */
    {
        const uint8_t *src = (const uint8_t *) "/* abc";
        AZOTokenizer tokenizer;
        AZOToken token = {0, 0, AZO_TOKEN_NONE};
        azo_tokenizer_setup(&tokenizer, src, 6);
        unsigned int ok = azo_tokenizer_get_next_token(&tokenizer, &token);
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_EQUAL_HEX(AZO_TOKEN_INVALID, token.type);
        TEST_ASSERT_EQUAL_UINT(0, token.start);
        TEST_ASSERT_EQUAL_UINT(6, token.end);
        azo_tokenizer_release(&tokenizer);
    }
    /* Invalid UTF-8 produces a non-empty invalid token */
    {
        const uint8_t src[] = {0xff, 0xfe};
        AZOTokenizer tokenizer;
        AZOToken token = {0, 0, AZO_TOKEN_NONE};
        azo_tokenizer_setup(&tokenizer, src, 2);
        unsigned int ok = azo_tokenizer_get_next_token(&tokenizer, &token);
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_EQUAL_HEX(AZO_TOKEN_INVALID, token.type);
        TEST_ASSERT_GREATER_THAN_UINT(token.start, token.end);
        azo_tokenizer_release(&tokenizer);
    }
}
