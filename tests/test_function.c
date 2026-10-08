/* Parser tests */

#include <string.h>

#include <az/az.h>
#include <az/object.h>
#include <az/extend.h>
#include <az/function.h>
#include <az/io/buffer-output-stream.h>
#include <az/io/os-output-stream.h>

#include <azo/source.h>
#include <azo/parser.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/operator.h>
#include <azo/errors.h>
#include <azo/context.h>
#include <azo/compiler/resolver.h>
#include <azo/compiler/optimizer.h>

#include "unity/unity.h"
#include "test.h"

typedef struct _TestObject TestObject;
typedef struct _TestObjectClass TestObjectClass;

#define TYPE_TESTOBJ test_object_get_type()

unsigned int test_object_get_type();

static unsigned int print_tree = 0;
static unsigned int print_bytecode = 0;

static AZODataBlock static_data = {0};
static AZOContext *globals = NULL;
static AZBufferOutputStream *bostream = NULL;
static AZOSOutputStream *osostream = NULL;

static int
test_program(AZOContext *ctx, const char *text, const AZImplementation *this_impl, void *this_inst,
	unsigned int n_args, const char *arg_names[], const AZImplementation *arg_impls[], const AZValue *arg_vals[],
	const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val)
{
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) text, strlen(text));

	AZOParser parser;
	azo_parser_setup (&parser, src);
	AZONode *tree = azo_parser_parse(&parser);
	if (print_tree) {
        fprintf(stderr, "Parser tree:\n");
		azo_node_print_info(tree, stderr, src, 0);
	}

    AZOCompiler comp;
	azo_compiler_setup(&comp, globals, src);
    comp.debug = 1;

	AZOCompilerContext comp_ctx = {
		.ret_type = ret_type
	};
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
	for (unsigned int i = 0; i < n_args; ++i) {
		AZString *str = az_string_new((const uint8_t *) arg_names[i]);
		azo_frame_declare_variable(comp_ctx.frame, str, AZ_IMPL_TYPE(arg_impls[i]));
	}

	int result = azo_compiler_resolve_program(&comp, &comp_ctx, tree);
    if (print_tree) {
        fprintf(stderr, "Resolved tree:\n");
    	azo_node_print_info(tree, stderr, src, 0);
    }
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return 1;
	}
	//azo_node_print_info(tree, stderr, src, 0);
	AZOOptimizer opt;
	azo_optimizer_setup(&opt, &comp);
	result = azo_compiler_optimize_program(&opt, &comp_ctx, tree, AZO_OPTIMIZER_FLAG_ALL);
	azo_optimizer_release(&opt);
    if (print_tree) {
        fprintf(stderr, "Optimized tree:\n");
       	azo_node_print_info(tree, stderr, src, 0);
    }
	if (result != 0) {
		azo_parser_release (&parser);
		azo_source_unref(src);
		azo_compiler_release(&comp);
		return 1;
	}

	AZOProgram *prog = azo_compiler_compile (&comp, &comp_ctx, tree, src);
    if (!prog) return 1;
    if (print_bytecode) azo_program_print_bytecode(prog);

    azo_interpreter_init(ctx->intr);
    azo_program_interpret(prog, ctx->intr, &static_data, n_args, arg_impls, arg_vals, ret_impl, ret_val, AZ_VALUE_MAX_SIZE);
    azo_program_unref(prog);
    az_object_unref((AZObject *) src);
    return 0;
}

static const char *simple_src = ""
"any func = (int32 i) int32 => { return i; };\n"
"return func(42);\n";

static const char *simple_src2 = ""
"any func = (int32 i) int32 => i;\n"
"return func(425);\n";

static const char *function_src = ""
"any a = (int32 a, int32 b) int32 => {\n"
"    for (int32 i = 0; i < b; i++) a = a + 1;\n"
"    return a;\n"
"};\n"
"int32 c = a(100, 28);\n"
"return c;\n";

void
test_function(void)
{
    az_init();
    globals = azo_context_new();
    azo_context_define_basic_types(globals);
    azo_context_define_class_by_str(globals, (const uint8_t *) "TestObject", TYPE_TESTOBJ);
    bostream = (AZBufferOutputStream *) az_instance_new(AZ_TYPE_BUFFER_OUTPUT_STREAM);
    osostream = (AZOSOutputStream *) az_instance_new(AZ_TYPE_OS_OUTPUT_STREAM);
    osostream->file = stdout;

    const AZImplementation *arg_impls[16];
	const AZValue *arg_vals[16];
    AZValue vals[16];

    arg_impls[0] = AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM);
    az_value_set_from_inst(AZ_IMPL_FROM_TYPE(AZ_TYPE_BUFFER_OUTPUT_STREAM), &vals[0], bostream);
    arg_vals[0] = &vals[0];
    arg_impls[1] = AZ_IMPL_FROM_TYPE(AZ_TYPE_OS_OUTPUT_STREAM);
    az_value_set_from_inst(AZ_IMPL_FROM_TYPE(AZ_TYPE_OS_OUTPUT_STREAM), &vals[1], osostream);
    arg_vals[1] = &vals[1];
	arg_impls[2] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
	vals[2].int32_v = 31;
	arg_vals[2] = &vals[2];
	arg_impls[3] = AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32);
	vals[3].int32_v = 3;
	arg_vals[3] = &vals[3];
	const char *arg_names[] = {"bofs", "ofs", "a_i32", "b_i32"};

	/* Simple program runs */
    {
        print_tree = 0;
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, simple_src, NULL, NULL, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(42, ret_val.int32_v);
        print_tree = 0;
    }
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, simple_src2, NULL, NULL, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(425, ret_val.int32_v);
    }
	/* Multiple variables */
    {
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, function_src, NULL, NULL, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_INT32, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), ret_impl);
        TEST_ASSERT_EQUAL_INT(128, ret_val.int32_v);
    }
    /* Capturing lambda */
    {
        print_tree = 0;
        static const char *src =
            "function test = () => {\n"
            "    int32 a = 1;\n"
            "    int32 b = 2;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "    a = 3;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "};\n"
            "test();\n";
		az_buffer_output_stream_reset(bostream);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, NULL, NULL, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("a + b = 3\na + b = 5\n", str);
        free(str);
        print_tree = 0;
    }
    /* The same program with global this */
    {
        static const char *src =
            "function test = () => {\n"
            "    int32 a = 1;\n"
            "    int32 b = 2;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "    a = 3;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "};\n"
            "test();\n";
		az_buffer_output_stream_reset(bostream);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        int32_t this_val = 42;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &this_val, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("a + b = 3\na + b = 5\n", str);
        free(str);
    }
    /* The same program in this context */
    {
        static const char *src =
            "a_i32 {\n"
            "function test = () => {\n"
            "    int32 a = 1;\n"
            "    int32 b = 2;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "    a = 3;\n"
            "    bofs.print(\"a + b = \");\n"
            "    bofs.printLn(a + b);\n"
            "};\n"
            "test();\n"
            "}\n";
		az_buffer_output_stream_reset(bostream);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, NULL, NULL, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("a + b = 3\na + b = 5\n", str);
        free(str);
    }
    /* Assigning property function with full signature and calling with implicit 'this' succeeds */
    {
        static const char *src =
            "print_tobj_i32 = (any x, int32 val) => {\n"
            "    ofs.printLn(val);\n"
            "};\n"
            "print_tobj_i32(42);\n";
        TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(TYPE_TESTOBJ), tobj, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_NONE, globals->intr->exc.type);
    }
    /* Assigning property with clearly wrong signature fails */
    {
        static const char *src =
            "print_tobj_i32 = (any x, int32 val, float val2) => {\n"
            "    ofs.printLn(val);\n"
            "};\n"
            "print_tobj_i32(42);\n";
        TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(TYPE_TESTOBJ), tobj, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        TEST_ASSERT_EQUAL_UINT32(AZO_EXCEPTION_INVALID_VALUE, globals->intr->exc.type);
    }
    /* A complex program */
    {
        static const char *src =
            "any all = {1, 2, 3, 4, 5, 6, 7 ,8, 9, 10};\n"
            "cnt = (any set) => {\n"
            "    for (uint32 i = 0; i < all.length; i++) {\n"
            "        bofs.print(set.contains(all[i]));\n"
            "        bofs.print(\" \");\n"
            "    }\n"
            "};\n"
            "cnt({2, 4, 6, 7, 9});\n";
		az_buffer_output_stream_reset(bostream);
        TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(TYPE_TESTOBJ), tobj, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("false true false true false true true false true false ", str);
        free(str);
    }
    /* new TestObject("Hello") */
    {
        print_tree = 0;
        print_bytecode = 0;
        static const char *src =
            "TestObject tobj = new TestObject(\"Hello\");\n"
            "bofs.print(tobj.text);\n";
		az_buffer_output_stream_reset(bostream);
        TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(TYPE_TESTOBJ), tobj, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("Hello", str);
        free(str);
    }
    /* new TestObject("Hello") */
    {
        print_tree = 0;
        print_bytecode = 0;
        static const char *src =
            "all = { 1, 2, 3, 4, 5 };\n"
            "odd = { 1, 3, 5 };\n"
            "function wearsAny = (any set) boolean => {\n"
            "for (uint32 i = 0; i < all.length; i++) {\n"
            "    if (set.contains(set[i])) return true;\n"
            "}\n"
            "return false;\n"
            "};\n"
            "find = (any obj, any set) => {\n"
            "  for (uint32 i = 0; i < all.length; i++) {\n"
            "    bofs.print(set.contains(all[i]));\n"
            "    bofs.print(\" \");\n"
            "  }\n"
            "};\n"
            "find (odd);\n";
		az_buffer_output_stream_reset(bostream);
        TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
        const AZImplementation *ret_impl;
        AZValue ret_val;
        TEST_ASSERT(test_program(globals, src, AZ_IMPL_FROM_TYPE(TYPE_TESTOBJ), tobj, 4, arg_names, arg_impls, arg_vals, AZ_TYPE_NONE, &ret_impl, &ret_val) == 0);
        TEST_ASSERT_EQUAL_PTR(NULL, ret_impl);
        char *str = strndup((const char *) bostream->buffer, bostream->pos);
        fprintf(stderr, "Output: %s\n", str);
        TEST_ASSERT_EQUAL_STRING("true false true false true ", str);
        free(str);
    }
}

struct _TestObject {
	AZActiveObject object;

    AZString *text;

    AZPackedValue onPrint1;
	AZPackedValue onPrint2;
};

struct _TestObjectClass {
	AZActiveObjectClass object_class;
};

static void test_object_class_init (TestObjectClass *klass);
static void test_object_finalize(const TestObjectClass *klass, TestObject *tobj);

static AZFunctionSignature *sig_i32 = NULL;
static AZFunctionSignature *sig_tobj_i32 = NULL;

/* Properties */
enum {
    FUNC_NEW_STR,
	FUNC_PRINT_I32,
	FUNC_PRINT_TOBJ_I32,
    PROP_TEXT,
	NUM_PROPERTIES
};

unsigned int
test_object_get_type (void)
{
	static unsigned int type = 0;
	if (!type) {
		AZClass *klass = az_register_type (&type, (const unsigned char *) "TestObject", AZ_TYPE_ACTIVE_OBJECT, sizeof (TestObjectClass), sizeof (TestObject), 0,
            0, NUM_PROPERTIES,
			(void (*) (AZClass *)) test_object_class_init,
			NULL,
			(void (*) (const AZImplementation *, void *)) test_object_finalize);
	}
	return type;
}

static void
test_object_finalize(const TestObjectClass *klass, TestObject *tobj)
{
	if (tobj->text) az_string_unref(tobj->text);
}

static TestObject *
test_object_new_str (AZString *key)
{
	TestObject *tobj = (TestObject *) az_object_new(TYPE_TESTOBJ);
    tobj->text = key;
    az_string_ref(key);
    return tobj;
}

static void
test_object_class_init (TestObjectClass *klass)
{
    sig_i32 = az_function_signature_new_va(AZ_TYPE_NONE, 1, AZ_TYPE_INT32);
    sig_tobj_i32 = az_function_signature_new_va(AZ_TYPE_NONE, 2, TYPE_TESTOBJ, AZ_TYPE_INT32);
	/* Properties */
	az_class_define_static_method_native_va ((AZClass *) klass, FUNC_NEW_STR, (const unsigned char *) "new", (void (*)()) test_object_new_str, TYPE_TESTOBJ, 1, AZ_TYPE_STRING);
	az_class_define_property_function_packed ((AZClass *) klass, FUNC_PRINT_I32, (const unsigned char *) "print_i32", 0,
        AZ_FIELD_INSTANCE, AZ_FIELD_READ_PACKED, AZ_FIELD_WRITE_PACKED,
		ARIKKEI_OFFSET(TestObject,onPrint1), sig_i32);
	az_class_define_property_function_packed ((AZClass *) klass, FUNC_PRINT_TOBJ_I32, (const unsigned char *) "print_tobj_i32", 0,
        AZ_FIELD_INSTANCE, AZ_FIELD_READ_PACKED, AZ_FIELD_WRITE_PACKED,
		ARIKKEI_OFFSET(TestObject,onPrint2), sig_tobj_i32);
    az_class_define_property_value((AZClass *) klass, PROP_TEXT, (const uint8_t *) "text", AZ_TYPE_STRING, 0, AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_VALUE, ARIKKEI_OFFSET(TestObject, text));
}
