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
        rctx.frame = azo_compiler_push_frame(&comp, NULL, NULL, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, NULL, NULL);
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
    /* Program with this class creates class context */
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
        rctx.frame = azo_compiler_push_frame(&comp, AZ_IMPL_FROM_TYPE(AZ_TYPE_OBJECT), NULL, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, AZ_IMPL_FROM_TYPE(AZ_TYPE_OBJECT), NULL);
        TEST_ASSERT(!result);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> CONTEXT -> TYPE, DECLARATION_LIST -> TYPE, DECLARATION -> REFERENCE(a), CONSTANT(1) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONTEXT, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_TYPE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_OBJECT, nodes[2]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Program with this instance creates instance context */
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
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival);
        TEST_ASSERT(!result);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> CONTEXT -> TYPE, DECLARATION_LIST -> TYPE, DECLARATION -> REFERENCE(a), CONSTANT(1) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONTEXT, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_INT32, nodes[2]->term.subtype);
        TEST_ASSERT_EQUAL_INT32(42, nodes[2]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[3]->term.type);
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
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, NULL, NULL, 0, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, NULL, NULL);
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
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival, 0, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival);
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
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, NULL, NULL, 0, AZ_TYPE_INT32);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, NULL, NULL);
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
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, AZ_IMPL_FROM_TYPE(AZ_TYPE_INT32), &ival);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Lambda assigned to property creates context */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("myprop = () => { return; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);

        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        int32_t ival = 42;
        rctx.frame = azo_compiler_push_frame(&comp, AZ_IMPL_FROM_TYPE(AZ_TYPE_ACTIVE_OBJECT), &ival, 0, AZ_TYPE_NONE);
        int result = azo_compiler_resolve_program(&comp, &rctx, tree, AZ_IMPL_FROM_TYPE(AZ_TYPE_ACTIVE_OBJECT), NULL);
        TEST_ASSERT(!result);
        // azo_node_print_info(tree, stdout, src, 0);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> CONTEXT -> TYPE, ASSIGN ->
             REFERENCE -> KEYWORD(this), REFERENCE
             FUNCTIN -> TYPE, LIST, CONTEXT -> KEYWORD(this), BLOCK -> KEYWORD */
        TEST_ASSERT_EQUAL_UINT(14, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_THIS, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONTEXT, nodes[10]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[11]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_THIS, nodes[11]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[12]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
}
