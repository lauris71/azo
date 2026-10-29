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
#include <azo/compiler/variable.h>

#include "unity/unity.h"
#include "test.h"

/* Resolve a program text against globals, returning the resolver result (0 = success) */
static int
resolve_text(AZOContext *globals, const char *text, AZOParser *parser, AZOSource **src, AZONode **tree)
{
    *tree = parse_text(text, parser, src);
    if (!*tree) return 1;
    AZOCompiler comp;
    azo_compiler_setup(&comp, globals, *src);
    AZOResolveCtx rctx = {
        .ret_type = AZ_TYPE_NONE
    };
    rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
    rctx.this_variant = AZO_COMPILER_NO_THIS;
    int result = azo_compiler_resolve_program(&comp, &rctx, *tree);
    azo_compiler_release(&comp);
    return result;
}

static const AZOExceptionDef *
lookup_exception_by_str(AZOContext *ctx, const char *name)
{
    AZString *str = az_string_new((const uint8_t *) name);
    const AZOExceptionDef *def = azo_context_lookup_exception(ctx, str);
    az_string_unref(str);
    return def;
}

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
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_LOCAL, nodes[4]->term.subtype);
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
        rctx.this_variant = AZO_COMPILER_THIS_IS_STACK;
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
        AZOVariable *var = azo_frame_declare_variable(rctx.frame, str, AZ_TYPE_ACTIVE_OBJECT, 0);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        //azo_node_print_info(tree, stdout, src, 0);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN ->
             REFERENCE -> VARIABLE_LOCAL(a), REFERENCE_MEMBER
             FUNCTION -> TYPE, LIST, MEMBER_BLOCK -> VARIABLE_CAPTURE(this), BLOCK -> RETURN */
        TEST_ASSERT_EQUAL_UINT(13, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_LOCAL, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_MEMBER_BLOCK, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, nodes[9]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_CAPTURE, nodes[9]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[10]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Return tracking: both if/else branches return - function returns */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = (int32 x) int32 => { if (x != 0) return 1; else return 2; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Return tracking: if without else does not guarantee return */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = (int32 x) int32 => { if (x != 0) return 1; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Return tracking: cycles do not guarantee return */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = (int32 x) int32 => { while (x != 0) return 1; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Scope tracking: if branches are separate scopes */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = (int32 x) int32 => { if (x != 0) int32 a = 1; else a = 2; return x; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Final tracking: final with initializer resolves */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { final int32 a = 1; return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Final tracking: final without initializer fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { final int32 a; return 0; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Final tracking: reassignment of final fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { final int32 a = 1; a = 2; return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Assignment type check: matching types resolve */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { int32 a = 1; int32 b = 2; a = b; return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Assignment type check: incompatible types fail */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { int32 a = 1; string s = \"x\"; a = s; return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Assignment type check: null to primitive fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { int32 a = 1; a = null; return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Assignment type check: null to block resolves */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { string s = \"x\"; s = null; return 0; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(!result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Condition check: non-boolean condition fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = (int32 x) int32 => { if (x) return 1; return 0; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 1, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Type propagation: unary minus on string yields string, rejected for int32 target */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { string s = \"x\"; int32 b = 0; b = -s; return b; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Kind check: arithmetic condition fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { int32 a = 1; int32 b = 2; if (a + b) return 1; return 0; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Kind check: arithmetic operand of && fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () int32 => { boolean a = true; int32 b = 1; int32 c = 2; if (a && (b + c)) return 1; return 0; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZOCompiler comp;
        azo_compiler_setup(&comp, globals, src);
        AZOResolveCtx rctx = {
            .ret_type = AZ_TYPE_NONE
        };
        rctx.frame = azo_compiler_new_frame(&comp, NULL, 0, 0, AZ_TYPE_NONE);
        rctx.this_variant = AZO_COMPILER_NO_THIS;
        int result = azo_compiler_resolve_program(&comp, &rctx, tree);
        TEST_ASSERT(result);
        azo_compiler_release(&comp);
        free_parse(&parser, src, tree);
    }
    /* Exception category declaration registers and becomes EMPTY */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "exception ResolverTestCategory;", &parser, &src, &tree);
        TEST_ASSERT(!result);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *decl = tree->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, decl->term.type);
        const AZOExceptionDef *def = lookup_exception_by_str(globals, "ResolverTestCategory");
        TEST_ASSERT_NOT_NULL(def);
        TEST_ASSERT_EQUAL_UINT(AZ_EXCEPTION_NONE, def->type);
        TEST_ASSERT(def->category != 0);
        free_parse(&parser, src, tree);
    }
    /* Exception type declaration registers with the category mask */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "exception ResolverTestException : ResolverTestCategory, IOException;", &parser, &src, &tree);
        TEST_ASSERT(!result);
        const AZOExceptionDef *def = lookup_exception_by_str(globals, "ResolverTestException");
        TEST_ASSERT_NOT_NULL(def);
        TEST_ASSERT(def->type != AZ_EXCEPTION_NONE);
        const AZOExceptionDef *cat1 = lookup_exception_by_str(globals, "ResolverTestCategory");
        const AZOExceptionDef *cat2 = lookup_exception_by_str(globals, "IOException");
        TEST_ASSERT_EQUAL_UINT64(cat1->category | cat2->category, def->category);
        free_parse(&parser, src, tree);
    }
    /* Idempotent re-declaration succeeds */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "exception ResolverTestException : ResolverTestCategory, IOException;", &parser, &src, &tree);
        TEST_ASSERT(!result);
        free_parse(&parser, src, tree);
    }
    /* Re-declaration with different categories fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "exception ResolverTestException : IOException;", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Type name conflicting with category fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "exception ResolverTestCategory : IOException;", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Exception declaration inside a block fails (program scope only) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "{ exception ScopedCategory; }", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Try/catch: binding becomes VARIABLE at the sentinel position, filters become constants */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { int32 a = 1; } catch (e : ResolverTestException, IOException) { int32 b = 2; }", &parser, &src, &tree);
        TEST_ASSERT(!result);
        AZONode *try_node = tree->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, try_node->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_TRY, try_node->term.subtype);
        AZONode *clause = try_node->children->next;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CATCH, clause->term.type);
        /* Binding: final exception-typed variable at the sentinel position */
        AZONode *binding = clause->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, binding->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE_LOCAL, binding->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_VARIABLE_FLAG_FINAL, binding->term.flags & AZO_VARIABLE_FLAG_FINAL);
        TEST_ASSERT_EQUAL_UINT(AZO_TYPE_EXCEPTION, binding->type);
        TEST_ASSERT_EQUAL_UINT(try_node->var_idx, binding->var_idx);
        /* Filters: type -> UINT32 code, category -> UINT64 mask bit */
        AZONode *f1 = clause->children->next->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, f1->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_UINT32, f1->type);
        TEST_ASSERT_EQUAL_UINT(lookup_exception_by_str(globals, "ResolverTestException")->type, f1->value.v.uint32_v);
        AZONode *f2 = f1->next;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, f2->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_UINT64, f2->type);
        TEST_ASSERT_EQUAL_UINT64(lookup_exception_by_str(globals, "IOException")->category, f2->value.v.uint64_v);
        free_parse(&parser, src, tree);
    }
    /* Catch-all not in last position fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { } catch (e) { } catch (e : IOException) { }", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Unknown exception in filter fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { } catch (e : NoSuchException) { }", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Try/catch stack layout: body locals after sentinel+handles, catch locals after the binding */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { int32 a = 1; } catch (e : IOException, ArithmeticException) { int32 b = 2; }", &parser, &src, &tree);
        TEST_ASSERT(!result);
        AZONode *try_node = tree->children;
        unsigned int sentinel = try_node->var_idx;
        /* 2 filter alternatives + sentinel = 3 reserved positions; body local follows them */
        AZONode *body = try_node->children;
        AZONode *a_var = body->children->children->next->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, a_var->term.type);
        TEST_ASSERT_EQUAL_UINT(sentinel + 3, a_var->var_idx);
        /* Catch binding at the sentinel position, catch local right after it */
        AZONode *clause = try_node->children->next;
        AZONode *binding = clause->children;
        TEST_ASSERT_EQUAL_UINT(sentinel, binding->var_idx);
        AZONode *cbody = clause->children->next->next;
        AZONode *b_var = cbody->children->children->next->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_VARIABLE, b_var->term.type);
        TEST_ASSERT_EQUAL_UINT(sentinel + 1, b_var->var_idx);
        free_parse(&parser, src, tree);
    }
    /* Catch binding is not visible in the try body */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { e = 1; } catch (e : IOException) { }", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Throw constructor rewrites to the fixed structure */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "throw ResolverTestException(\"bad\", 42);", &parser, &src, &tree);
        TEST_ASSERT(!result);
        AZONode *thr = tree->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, thr->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_THROW, thr->term.subtype);
        /* CODE(U32), MESSAGE, PAYLOAD (mask is looked up from the registry at throw time) */
        AZONode *code = thr->children;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, code->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_UINT32, code->type);
        TEST_ASSERT_EQUAL_UINT(lookup_exception_by_str(globals, "ResolverTestException")->type, code->value.v.uint32_v);
        AZONode *msg = code->next;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, msg->term.type);
        TEST_ASSERT_EQUAL_UINT(AZ_TYPE_STRING, msg->type);
        AZONode *payload = msg->next;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, payload->term.type);
        TEST_ASSERT_EQUAL_INT32(42, payload->value.v.int32_v);
        TEST_ASSERT_NULL(payload->next);
        free_parse(&parser, src, tree);
    }
    /* Throw constructor with no arguments gets EMPTY message/payload */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "throw ResolverTestException();", &parser, &src, &tree);
        TEST_ASSERT(!result);
        AZONode *thr = tree->children;
        AZONode *msg = thr->children->next;
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, msg->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, msg->next->term.type);
        TEST_ASSERT_NULL(msg->next->next);
        free_parse(&parser, src, tree);
    }
    /* Throw of unregistered name fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "throw NoSuchException();", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Throw of a category (not a type) fails */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "throw IOException();", &parser, &src, &tree);
        TEST_ASSERT(result);
        if (tree) free_parse(&parser, src, tree);
    }
    /* Rethrow inside a catch clause resolves */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree;
        int result = resolve_text(globals, "try { } catch (e : IOException) { throw e; }", &parser, &src, &tree);
        TEST_ASSERT(!result);
        free_parse(&parser, src, tree);
    }
}
