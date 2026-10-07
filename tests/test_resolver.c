/* Parser tests */

#include <string.h>

#include <az/az.h>
#include <az/object.h>

#include <azo/source.h>
#include <azo/parser.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/operator.h>
#include <azo/errors.h>
#include <azo/context.h>
#include <azo/compiler/resolver.h>

#include "unity/unity.h"
#include "test.h"

void
test_resolver(void)
{
    az_init();
    AZOContext *globals = azo_context_new();
    azo_context_define_basic_types(globals);
    unsigned int result = 0;
    /* Simple program resolves */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECLARATION_LIST -> TYPE, DECLARATION -> REFERENCE(a), CONSTANT(1) */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_TYPE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[5]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'this' in static program fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return this;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'this' in member program succeeds */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return this;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_INT32);
        AZOVariable *var = azo_frame_declare_this(rctx.frame, AZ_TYPE_INT32);
        rctx.this_variant = AZO_COMPILER_THIS_IS_ARGUMENT;
        rctx.this_var_pos = var->pos;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'this' in member block succeeds */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1; a { return this; }", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'this' in static context fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1; a { static { return this; }}", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        azo_frame_declare_this(rctx.frame, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'this' in nested member context succeeds */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1; a { static { a { return this; }}}", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        azo_frame_declare_this(rctx.frame, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* 'new' is resolved to function call of the class */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return new int32(1,2,3);", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        //azo_node_print_info(tree, stdout, src, 0);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        azo_frame_declare_this(rctx.frame, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        //azo_node_print_info(tree, stdout, src, 0);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(9, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_CALL, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_CALL_PROPERTY, nodes[2]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE_MEMBER, nodes[4]->term.subtype);
        TEST_ASSERT(nodes[4]->value.v.block = AZ_CLASS_FROM_TYPE(AZ_TYPE_INT32));

        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Lambda assigned to property creates context */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a.myprop = () => { return this; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        //azo_node_print_info(tree, stdout, src, 0);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        AZString *str = az_string_new((const uint8_t *) "a");
        AZOVariable *var = azo_frame_declare_variable(rctx.frame, str, AZ_TYPE_ACTIVE_OBJECT);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        //azo_node_print_info(tree, stdout, src, 0);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> CONTEXT -> TYPE, ASSIGN ->
             REFERENCE -> KEYWORD(this), REFERENCE
             FUNCTIN -> TYPE, LIST, CONTEXT -> KEYWORD(this), BLOCK -> KEYWORD */
        TEST_ASSERT_EQUAL_UINT(13, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_LOCAL, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONTEXT, nodes[9]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, nodes[10]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_CAPTURE, nodes[10]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
}
