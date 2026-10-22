#ifndef AZO_TEST_H
#define AZO_TEST_H

#include <stdint.h>
#include <azo/parser.h>
#include <azo/source.h>
#include <azo/node.h>
#include <azo/context.h>

/* Shared test helpers */

/* Compile and run a program, returning the int32 result */
int32_t run_program(const char *source);
/* Compile and run generic program with arguments and return type */
int test_program(AZOContext *ctx, const char *text,
	unsigned int n_args, const char *arg_names[], const AZImplementation *arg_impls[], const AZValue *arg_vals[],
	const unsigned int ret_type, const AZImplementation **ret_impl, AZValue *ret_val);

/* Parse a text into a syntax tree (parser and src are filled in) */
AZONode *parse_text(const char *text, AZOParser *parser, AZOSource **src);

/* Free a parse tree, the parser and the source */
void free_parse(AZOParser *parser, AZOSource *src, AZONode *tree);

/* Test entry points (called from main in test.c) */
void test_tokenizer(void);
void test_parser(void);
void test_resolver(void);
void test_compile(void);
void test_language(void);
void test_assign(void);
void test_comparison(void);
void test_cycles(void);
void test_function(void);
void test_calculate(void);
void test_calculate_binary(void);

#endif /* AZO_TEST_H */
