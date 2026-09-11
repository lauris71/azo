/* Parser tests */

#include <string.h>

#include <az/az.h>
#include <az/object.h>
#include <azo/source.h>
#include <azo/parser.h>
#include <azo/node.h>
#include <azo/keyword.h>
#include <azo/operator.h>

#include "unity/unity.h"
#include "test.h"

AZONode *
parse_text(const char *text, AZOParser *parser, AZOSource **src)
{
    *src = azo_source_new_static((const uint8_t *) "test", (const uint8_t *) text, strlen(text));
    azo_parser_setup(parser, *src);
    return azo_parser_parse(parser);
}

void
free_parse(AZOParser *parser, AZOSource *src, AZONode *tree)
{
    if (tree) azo_node_free_tree(tree);
    azo_parser_release(parser);
    az_object_unref((AZObject *) src);
}

void
test_parser(void)
{
    az_init();
    /* Flatten simple assignment tree */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = 1;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(4, n);
        /* PROGRAM -> ASSIGN(=) -> REFERENCE(a), CONSTANT(1) */
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN_PLAIN, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[3]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Syntax error is logged into the error list */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = ;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION, parser.errors[0].code);
        TEST_ASSERT_EQUAL_STRING(azo_parser_error_get_message(AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION), parser.errors[0].message);
        TEST_ASSERT_EQUAL_STRING("Unknown error", azo_parser_error_get_message(999));
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Recovery: garbage sentence is replaced by INVALID node, parsing continues */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("{ %%%; b = 2; }", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> BLOCK -> INVALID, ASSIGN(b = 2) */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_INVALID, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_INT32(2, nodes[5]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Recovery at top level: poison node replaces the garbage, next line parses */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = ; b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> INVALID(a = ...), ASSIGN(b = 2) */
        TEST_ASSERT_EQUAL_UINT(5, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_INVALID, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_INT32(2, nodes[4]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Missing semicolon before } is repaired without consuming the brace */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("{ a = 1 }", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> BLOCK -> ASSIGN(a = 1) - no poison node */
        TEST_ASSERT_EQUAL_UINT(5, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[4]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Missing semicolon before else is repaired, if-else parses completely */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("if (a) b = 1 else b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> IF(a, ASSIGN(b = 1), ASSIGN(b = 2)) */
        TEST_ASSERT_EQUAL_UINT(9, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_IF, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[5]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_INT32(2, nodes[8]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Block node span covers the braces */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = 0; { a = 1; } y = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[4]->term.type);
        /* { is at offset 7, } ends at offset 17 */
        TEST_ASSERT_EQUAL_UINT(7, nodes[4]->term.start);
        TEST_ASSERT_EQUAL_UINT(17, nodes[4]->term.end);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* do STATEMENT while (EXPRESSION) ; */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("do a = a + 1; while (a < 100);", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DO(ASSIGN(a = a + 1), COMPARISON(a < 100)) */
        TEST_ASSERT_EQUAL_UINT(10, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_DO, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BINARY, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARITHMETIC_PLUS, nodes[4]->term.subtype);
        TEST_ASSERT_EQUAL_INT32(1, nodes[6]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_COMPARISON, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_COMPARISON_LT, nodes[7]->term.subtype);
        TEST_ASSERT_EQUAL_INT32(100, nodes[9]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* do-while requires semicolon, missing one is repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("do a = 1; while (a < 2) b = 3;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DO(...), ASSIGN(b = 3) */
        TEST_ASSERT_EQUAL_UINT(11, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_DO, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_INT32(3, nodes[10]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Parsing aborts when the error list is full */
    {
        AZOParser parser;
        AZOSource *src;
        /* 30 garbage sentences, error list caps at AZO_PARSER_MAX_ERRORS */
        AZONode *tree = parse_text("%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;%;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_MAX_ERRORS, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Multi_statement: comma-separated assignments in one line */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = 1, b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> STATEMENT_GROUP(ASSIGN(a = 1), ASSIGN(b = 2)) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_STATEMENT_GROUP, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[4]->value.v.int32_v);
        TEST_ASSERT_EQUAL_INT32(2, nodes[7]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Multi_statement: declaration with its own comma list is not split */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1, b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECLARATION_LIST(int32, DECL(a = 1), DECL(b = 2)) */
        TEST_ASSERT_EQUAL_UINT(9, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Multi_statement: return cannot be an item */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return 1, 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Multi_statement: items cannot be empty */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = 1, ;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Multi_statement: missing semicolon after the last item is repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = 1, b = 2", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_STATEMENT_GROUP, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* For: multi-statements in the header are collected under statement groups */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("for (i = 0, j = 1; i < 3; i++, j--) b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[32];
        unsigned int n = azo_node_flatten(tree, nodes, 32);
        /* PROGRAM -> FOR(STATEMENT_GROUP(init...), COMPARISON, STATEMENT_GROUP(step...), ASSIGN) */
        TEST_ASSERT_EQUAL_UINT(20, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_FOR, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_STATEMENT_GROUP, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_COMPARISON, nodes[9]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_STATEMENT_GROUP, nodes[12]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_SUFFIX, nodes[13]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_SUFFIX, nodes[15]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[17]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* For: single header statements are not wrapped */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("for (i = 0; i < 3; i++) b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[32];
        unsigned int n = azo_node_flatten(tree, nodes, 32);
        TEST_ASSERT_EQUAL_UINT(13, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_FOR, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_COMPARISON, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_SUFFIX, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[10]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* For: condition may be omitted */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("for (i = 0;; i++) b = 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(11, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_FOR, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_SUFFIX, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* break and continue are simple keyword statements */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("while (a) { break; continue; }", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> FOR(EMPTY, a, EMPTY, BLOCK(BREAK, CONTINUE)) */
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_FOR, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_BREAK, nodes[6]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_CONTINUE, nodes[7]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* break without semicolon at EOF is repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("while (a) break", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_BREAK, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* debug is not part of the language spec (HAS_DEBUG_KEYWORD not defined) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("debug;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
#ifdef HAS_DEBUG_KEYWORD
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
#else
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
#endif
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* return with and without a value */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return 1; return;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> RETURN(1), RETURN(no children) */
        TEST_ASSERT_EQUAL_UINT(4, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[2]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[3]->term.subtype);
        TEST_ASSERT_NULL(nodes[3]->children);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* bare return before } gets the missing semicolon repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("{ return }", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(3, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[2]->term.subtype);
        TEST_ASSERT_NULL(nodes[2]->children);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* bare return at EOF is repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("return", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(2, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[1]->term.subtype);
        TEST_ASSERT_NULL(nodes[1]->children);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Qualifiers can appear in any order (compiler ignores them, parser keeps them) */
    {
        const char *srcs[] = {
            "static const final int32 a = 1;",
            "const static int32 a = 1;",
            "final static const int32 a = 1;",
            "const int32 a = 1;",
            NULL
        };
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            TEST_ASSERT_NOT_NULL(tree);
            AZONode *nodes[16];
            unsigned int n = azo_node_flatten(tree, nodes, 16);
            /* PROGRAM -> DECLARATION_LIST(int32, DECL(a = 1)) */
            TEST_ASSERT_EQUAL_UINT(6, n);
            TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
            TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
            TEST_ASSERT_EQUAL_INT32(1, nodes[5]->value.v.int32_v);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(0, parser.n_errors, srcs[i]);
            free_parse(&parser, src, tree);
        }
    }
    /* Each qualifier only once per declaration */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("static static int32 a = 1;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SYNTAX, parser.errors[0].code);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Qualified empty statement is an error */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("static;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* 'function' as a type in declaration */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECLARATION_LIST(function, DECL(f)) */
        TEST_ASSERT_EQUAL_UINT(5, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Keyword cannot be used as a variable name in a declaration list */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a, if = 1;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* for with all clauses empty */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("for (;;) b = 1;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> FOR(EMPTY, EMPTY, EMPTY, ASSIGN(b = 1)) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_FOR, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* 'function' as a type, initialized with a lambda */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = () void => { return; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECL_LIST(function, DECL(f, FUNCTION_STATIC(void, (), { return; }))) */
        TEST_ASSERT_EQUAL_UINT(10, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_VOID, nodes[6]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[9]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[9]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: untyped args, expression body */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (a, b) => a + b;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[24];
        unsigned int n = azo_node_flatten(tree, nodes, 24);
        /* PROGRAM -> ASSIGN(f, FUNCTION_STATIC(EMPTY, LIST(a, b), +(a, b))) */
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_EMPTY, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARGUMENT_DECLARATION, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BINARY, nodes[12]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARITHMETIC_PLUS, nodes[12]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: typed args + return type + block body */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (int32 a, int32 b) int32 => { return a + b; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[24];
        unsigned int n = azo_node_flatten(tree, nodes, 24);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARGUMENT_DECLARATION, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[12]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: empty args */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = () => 42;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_INT32(42, nodes[6]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: empty args with return type */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = () int32 => 42;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_INT32(42, nodes[6]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: empty args with void return type */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = () void => { return; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_VOID, nodes[4]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: typed args, member reference return type */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (int32 x) mod.Type => x;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE_MEMBER, nodes[4]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: typed args, function call return type */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (int32 x) factory() => x;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_CALL, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda: return type requires typed arguments */
    {
        const char *srcs[] = {
            "f = (x) int32 => x;",
            "f = (x) void => x;",
            "f = (a, b) int32 => a;",
            "f = (x) mod.Type => x;",
            NULL
        };
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            if (tree) azo_node_free_tree(tree);
            TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0, parser.n_errors, srcs[i]);
            azo_parser_release(&parser);
            az_object_unref((AZObject *) src);
        }
    }
    /* Lambda: arguments are either all typed or all untyped */
    {
        const char *srcs[] = {
            "f = (int32 a, b) => a;",
            "f = (a, int32 b) => a;",
            "f = (int32 a, b) int32 => a;",
            NULL
        };
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            if (tree) azo_node_free_tree(tree);
            TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0, parser.n_errors, srcs[i]);
            azo_parser_release(&parser);
            az_object_unref((AZObject *) src);
        }
    }
    /* Lambda: nested lambda as body */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (a) => (b) => a + b;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        AZONode *nodes[32];
        unsigned int n = azo_node_flatten(tree, nodes, 32);
        unsigned int n_lambdas = 0;
        for (unsigned int i = 0; i < n; i++) if (nodes[i]->term.type == AZO_TERM_FUNCTION) n_lambdas += 1;
        TEST_ASSERT_EQUAL_UINT(2, n_lambdas);
        free_parse(&parser, src, tree);
    }
    /* Cast of a member of a lambda (the motivating example) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("v = (int32) (int32 x) int32 => { return -x; }.numArgs;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        AZONode *nodes[32];
        unsigned int n = azo_node_flatten(tree, nodes, 32);
        unsigned int has_cast = 0, has_lambda = 0, has_member = 0;
        for (unsigned int i = 0; i < n; i++) {
            if (nodes[i]->term.type == AZO_TERM_CAST) has_cast = 1;
            if (nodes[i]->term.type == AZO_TERM_FUNCTION) has_lambda = 1;
            if ((nodes[i]->term.type == AZO_TERM_REFERENCE) && (nodes[i]->term.subtype == AZO_TERM_REFERENCE_MEMBER)) has_member = 1;
        }
        TEST_ASSERT_TRUE(has_cast);
        TEST_ASSERT_TRUE(has_lambda);
        TEST_ASSERT_TRUE(has_member);
        free_parse(&parser, src, tree);
    }
    /* Lambda as a call argument */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("mylist.sort((a, b) => a.compare(b));", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        AZONode *nodes[32];
        unsigned int n = azo_node_flatten(tree, nodes, 32);
        /* there is a FUNCTION (lambda) node in the tree */
        unsigned int has_lambda = 0;
        for (unsigned int i = 0; i < n; i++) if (nodes[i]->term.type == AZO_TERM_FUNCTION) has_lambda = 1;
        TEST_ASSERT_TRUE(has_lambda);
        free_parse(&parser, src, tree);
    }
    /* Parenthesized expression is not a lambda */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (a + b) * 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        unsigned int has_lambda = 0;
        for (unsigned int i = 0; i < n; i++) if (nodes[i]->term.type == AZO_TERM_FUNCTION) has_lambda = 1;
        TEST_ASSERT_FALSE(has_lambda);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Lambda with empty body is an error */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("f = (a) => ;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
#ifdef HAS_FUNCTION_KEYWORD
    /* LEGACY: anonymous function definition (old scripts, superseded by lambdas) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("any g = function int32 (int32 a) { return a; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECL_LIST(any, DECL(g, FUNCTION_STATIC(int32, (int32 a), { return a; }))) */
        TEST_ASSERT_EQUAL_UINT(14, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARGUMENT_DECLARATION, nodes[8]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[11]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[12]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_RETURN, nodes[12]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* LEGACY: 'function' as a type, initialized with a function definition */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("function f = function void (int32 a) { return; };", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECL_LIST(function, DECL(f, FUNCTION_STATIC(void, (int32 a), { return; }))) */
        TEST_ASSERT_EQUAL_UINT(13, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_STATIC, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_VOID, nodes[6]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[7]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BLOCK, nodes[11]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* LEGACY: member function definition - FUNCTION_MEMBER(type, obj, args, body) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("obj.function void m(int32 a) { return; };", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* LEGACY: the bareword evaluates to the function class */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("any h = function;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECL_LIST(any, DECL(h, REFERENCE(function))) */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE_VARIABLE, nodes[5]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
#endif
    /* Keywords cannot be used as references */
    {
        const char *srcs[] = {"a.else;", "a.static;", "a.return;", NULL};
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            if (tree) azo_node_free_tree(tree);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(1, parser.n_errors, srcs[i]);
            azo_parser_release(&parser);
            az_object_unref((AZObject *) src);
        }
    }
    /* Declaration list: mixed initializations */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a = 1, b = 2, c;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> DECL_LIST(int32, DECL(a = 1), DECL(b = 2), DECL(c)) */
        TEST_ASSERT_EQUAL_UINT(11, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_INT32(1, nodes[5]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[6]->term.type);
        TEST_ASSERT_EQUAL_INT32(2, nodes[8]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[9]->term.type);
        TEST_ASSERT_NULL(nodes[9]->children->next);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Trailing comma in declaration list is an error (also at EOF) */
    {
        const char *srcs[] = {"int32 a,;", "int32 a,", NULL};
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            if (tree) azo_node_free_tree(tree);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(1, parser.n_errors, srcs[i]);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(AZO_PARSER_ERROR_SYNTAX, parser.errors[0].code, srcs[i]);
            azo_parser_release(&parser);
            az_object_unref((AZObject *) src);
        }
    }
    /* Declaration at EOF without semicolon is repaired */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32 a", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(5, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SEMICOLON_MISSING, parser.errors[0].code);
        free_parse(&parser, src, tree);
    }
    /* Type alone is not a valid statement */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("int32;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Parenthesized expression (no cast) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (a + b) * 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(x, BINARY(*(BINARY(+)(a, b), 2))) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BINARY, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARITHMETIC_STAR, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BINARY, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARITHMETIC_PLUS, nodes[4]->term.subtype);
        TEST_ASSERT_EQUAL_INT32(2, nodes[7]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Cast: node span starts at the opening parenthesis */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (int32) a;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(x, CAST(int32, a)) - cast spans from '(' at offset 4 */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CAST, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(4, nodes[3]->term.start);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Error inside parenthesized expression propagates (not swallowed as a cast) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (a + ) b;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION, parser.errors[0].code);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Missing closing parenthesis is an error */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (a", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_UNEXPECTED_EOF, parser.errors[0].code);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Chained assignment: flat multi-target node */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = b = c = 0;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(a, b, c, 0) - targets followed by the value */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PROGRAM, nodes[0]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN_PLAIN, nodes[1]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_INT32(0, nodes[5]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Compound assignment does not chain */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a += b = 1;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Chain targets must be lvalues */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = f() = 1;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SYNTAX, parser.errors[0].code);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Assignment is not an expression (the C trap is closed) */
    {
        const char *srcs[] = {"f(a = 1);", "if (b = true) a = 1;", "x = (a = 1);", NULL};
        for (int i = 0; srcs[i]; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(srcs[i], &parser, &src);
            if (tree) azo_node_free_tree(tree);
            TEST_ASSERT_GREATER_OR_EQUAL_UINT_MESSAGE(1, parser.n_errors, srcs[i]);
            azo_parser_release(&parser);
            az_object_unref((AZObject *) src);
        }
    }
    /* Two-word primitive type cast */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (complex float) a;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CAST, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Parenthesized non-type is never a cast */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = (b) * 2;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(a, BINARY(*(b, 2))) - no CAST node */
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_BINARY, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ARITHMETIC_STAR, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Parenthesized primitive type alone is the class value, not a cast */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = (int32);", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(4, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Class casts are not C-style casts (use 'as' instead) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (MyClass) a;", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Two-word primitive type in declaration */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("complex float c = 1.0;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(6, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_DECLARATION, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Number literals: bases, suffixes, magnitudes */
    {
        const struct { const char *src; unsigned int az_type; int64_t val; } cases[] = {
            { "x = 42;", AZ_TYPE_INT32, 42 },
            { "x = 0x1F;", AZ_TYPE_INT32, 31 },
            { "x = 0b101;", AZ_TYPE_INT32, 5 },
            { "x = 42u;", AZ_TYPE_UINT32, 42 },
            { "x = 42l;", AZ_TYPE_INT64, 42 },
            { "x = 42ul;", AZ_TYPE_UINT64, 42 },
            { "x = 42lu;", AZ_TYPE_UINT64, 42 },
            { "x = 3000000000;", AZ_TYPE_INT64, 3000000000LL },       /* auto-widen */
            { "x = 3000000000u;", AZ_TYPE_UINT32, 3000000000LL },
            { "x = 0xFF00FF00;", AZ_TYPE_INT64, 0xFF00FF00LL },
            { NULL, 0, 0 }
        };
        for (int i = 0; cases[i].src; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(cases[i].src, &parser, &src);
            TEST_ASSERT_NOT_NULL(tree);
            AZONode *nodes[16];
            unsigned int n = azo_node_flatten(tree, nodes, 16);
            TEST_ASSERT_EQUAL_UINT(4, n);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(AZO_TERM_CONSTANT, nodes[3]->term.type, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(cases[i].az_type, nodes[3]->term.subtype, cases[i].src);
            if (cases[i].az_type == AZ_TYPE_INT32) TEST_ASSERT_EQUAL_INT32_MESSAGE((int32_t) cases[i].val, nodes[3]->value.v.int32_v, cases[i].src);
            else if (cases[i].az_type == AZ_TYPE_UINT32) TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) cases[i].val, nodes[3]->value.v.uint32_v, cases[i].src);
            else if (cases[i].az_type == AZ_TYPE_INT64) TEST_ASSERT_EQUAL_INT64_MESSAGE(cases[i].val, nodes[3]->value.v.int64_v, cases[i].src);
            else if (cases[i].az_type == AZ_TYPE_UINT64) TEST_ASSERT_EQUAL_UINT64_MESSAGE((uint64_t) cases[i].val, nodes[3]->value.v.uint64_v, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(0, parser.n_errors, cases[i].src);
            free_parse(&parser, src, tree);
        }
    }
    /* Negative integer is a prefix operator applied to a positive constant */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = -5;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(x, PREFIX(-, 5)) */
        TEST_ASSERT_EQUAL_UINT(5, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PREFIX, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_PREFIX_MINUS, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_CONSTANT, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_INT32(5, nodes[4]->value.v.int32_v);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* new and function behave like other expressions (trailing operators work) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("new MyObject().someMethod();", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> CALL(MEMBER(NEW(MyObject, args), someMethod), args) */
        TEST_ASSERT_EQUAL_UINT(8, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION_CALL, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[2]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE_MEMBER, nodes[2]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_NEW, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[5]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* new with a member-qualified class name */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = new a.b.C(1, 2);", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        TEST_ASSERT_EQUAL_UINT(12, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_KEYWORD, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_KEYWORD_NEW, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_LIST, nodes[9]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* member access on a lambda (function definition) */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("x = (() void => {}).class;", &parser, &src);
        TEST_ASSERT_NOT_NULL(tree);
        AZONode *nodes[16];
        unsigned int n = azo_node_flatten(tree, nodes, 16);
        /* PROGRAM -> ASSIGN(x, MEMBER(FUNCTION(...), class)) */
        TEST_ASSERT_EQUAL_UINT(9, n);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_ASSIGN, nodes[1]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE, nodes[3]->term.type);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_REFERENCE_MEMBER, nodes[3]->term.subtype);
        TEST_ASSERT_EQUAL_UINT(AZO_TERM_FUNCTION, nodes[4]->term.type);
        TEST_ASSERT_EQUAL_UINT(0, parser.n_errors);
        free_parse(&parser, src, tree);
    }
    /* Cast qualifiers set term flags */
    {
        const struct { const char *src; uint16_t flags; } cases[] = {
            { "x = (int16) a;", 0 },
            { "x = (int16 exact) a;", AZO_TERM_FLAG_EXACT },
            { "x = (int16 rounded) a;", AZO_TERM_FLAG_ROUNDED },
            { NULL, 0 }
        };
        for (int i = 0; cases[i].src; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(cases[i].src, &parser, &src);
            TEST_ASSERT_NOT_NULL(tree);
            AZONode *nodes[16];
            unsigned int n = azo_node_flatten(tree, nodes, 16);
            TEST_ASSERT_EQUAL_UINT(6, n);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(AZO_TERM_CAST, nodes[3]->term.type, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(cases[i].flags, nodes[3]->term.flags, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(0, parser.n_errors, cases[i].src);
            free_parse(&parser, src, tree);
        }
    }
    /* Cast qualifier without cast operand is an error */
    {
        AZOParser parser;
        AZOSource *src;
        AZONode *tree = parse_text("a = (int16 rounded);", &parser, &src);
        if (tree) azo_node_free_tree(tree);
        TEST_ASSERT_EQUAL_UINT(1, parser.n_errors);
        TEST_ASSERT_EQUAL_UINT(AZO_PARSER_ERROR_SYNTAX, parser.errors[0].code);
        azo_parser_release(&parser);
        az_object_unref((AZObject *) src);
    }
    /* Declaration qualifiers set term flags on the declaration list */
    {
        const struct { const char *src; uint16_t flags; } cases[] = {
            { "int32 a = 1;", 0 },
            { "static int32 a = 1;", AZO_TERM_FLAG_STATIC },
            { "const int32 a = 1;", AZO_TERM_FLAG_CONST },
            { "final int32 a = 1;", AZO_TERM_FLAG_FINAL },
            { "static const final int32 a = 1;", AZO_TERM_FLAG_STATIC | AZO_TERM_FLAG_CONST | AZO_TERM_FLAG_FINAL },
            { "const static int32 a = 1;", AZO_TERM_FLAG_STATIC | AZO_TERM_FLAG_CONST },
            { NULL, 0 }
        };
        for (int i = 0; cases[i].src; i++) {
            AZOParser parser;
            AZOSource *src;
            AZONode *tree = parse_text(cases[i].src, &parser, &src);
            TEST_ASSERT_NOT_NULL(tree);
            AZONode *nodes[16];
            unsigned int n = azo_node_flatten(tree, nodes, 16);
            TEST_ASSERT_EQUAL_UINT(6, n);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(AZO_TERM_DECLARATION_LIST, nodes[1]->term.type, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(cases[i].flags, nodes[1]->term.flags, cases[i].src);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(0, parser.n_errors, cases[i].src);
            free_parse(&parser, src, tree);
        }
    }
}
