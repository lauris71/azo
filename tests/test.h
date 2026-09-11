#ifndef AZO_TEST_H
#define AZO_TEST_H

#include <stdint.h>
#include <azo/parser.h>
#include <azo/source.h>
#include <azo/node.h>

/* Shared test helpers */

/* Compile and run a program, returning the int32 result */
int32_t run_program(const char *source);

/* Parse a text into a syntax tree (parser and src are filled in) */
AZONode *parse_text(const char *text, AZOParser *parser, AZOSource **src);

/* Free a parse tree, the parser and the source */
void free_parse(AZOParser *parser, AZOSource *src, AZONode *tree);

/* Test entry points (called from main in test.c) */
void test_compile(void);
void test_assign(void);
void test_comparison(void);
void test_function(void);
#ifdef HAS_FUNCTION_KEYWORD
void test_legacy_function(void);
#endif
void test_tokenizer(void);
void test_parser(void);

#endif /* AZO_TEST_H */
