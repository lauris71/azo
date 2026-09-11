#ifndef __AZO_KEYWORD_H__
#define __AZO_KEYWORD_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <stdio.h>

#include <azo/source.h>
#include <azo/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	AZO_KEYWORD_NONE,
	/* NULL */
	AZO_KEYWORD_NULL,
	/* VOID */
	AZO_KEYWORD_VOID,
	/* TYPE QUALIFIERS */
	AZO_KEYWORD_STATIC,
	AZO_KEYWORD_CONST,
	AZO_KEYWORD_FINAL,
	/* BOOLEANS */
	AZO_KEYWORD_TRUE,
	AZO_KEYWORD_FALSE,
	/* THIS */
	AZO_KEYWORD_THIS,
	/* FOR, INIT, CONDITION, STEP, STATEMENT */
	AZO_KEYWORD_FOR,
	/* WHILE, CONDITION, STATEMENT */
	AZO_KEYWORD_WHILE,
	/* DO, STATEMENT, CONDITION */
	AZO_KEYWORD_DO,
	/* CLASS, LIST */
	AZO_KEYWORD_NEW,
	/* CONDITION, TRUE_STATEMENT, FALSE_STATEMENT */
	AZO_KEYWORD_IF,
	AZO_KEYWORD_ELSE,
	/* RETURN [VALUE] */
	AZO_KEYWORD_RETURN,
	/* REFERENCE IS REFERENCE */
	AZO_KEYWORD_IS,
	/* REFERENCE IMPLEMENTS REFERENCE */
	AZO_KEYWORD_IMPLEMENTS,
	/* REFERENCE AS REFERENCE (class/interface conversion - same precedence as is/implements) */
	AZO_KEYWORD_AS,

	/* PRIMITIVE TYPE NAMES (keep contiguous - the cast rule needs a range check) */
	AZO_KEYWORD_BOOLEAN,
	AZO_KEYWORD_INT8,
	AZO_KEYWORD_UINT8,
	AZO_KEYWORD_INT16,
	AZO_KEYWORD_UINT16,
	AZO_KEYWORD_INT32,
	AZO_KEYWORD_UINT32,
	AZO_KEYWORD_INT64,
	AZO_KEYWORD_UINT64,
	AZO_KEYWORD_FLOAT,
	AZO_KEYWORD_DOUBLE,
	/* complex float / complex double (two-word type names) */
	AZO_KEYWORD_COMPLEX,
	AZO_KEYWORD_POINTER,
	AZO_KEYWORD_LAST_PRIMITIVE_TYPE = AZO_KEYWORD_POINTER,

	/* DEBUG */
	AZO_KEYWORD_DEBUG,
	/* BREAK */
	AZO_KEYWORD_BREAK,
	/* CONTINUE */
	AZO_KEYWORD_CONTINUE,

	/* CAST QUALIFIERS */
	AZO_KEYWORD_EXACT,
	AZO_KEYWORD_ROUNDED,

#ifdef HAS_FUNCTION_KEYWORD
	/* LEGACY - function definitions are written as lambdas (=>), the keyword is kept for old scripts */
	/* Keep last - removing the guard must not shift other keyword values */
	/* [VALUE.] FUNCTION, [TYPE], LIST, STATEMENT */
	AZO_KEYWORD_FUNCTION,
#endif

	AZO_NUM_KEYWORDS
};

/* Whether the keyword is a primitive type name (the target of a C-style cast) */
#define AZO_KEYWORD_IS_PRIMITIVE_TYPE(kw) (((kw) >= AZO_KEYWORD_BOOLEAN) && ((kw) <= AZO_KEYWORD_LAST_PRIMITIVE_TYPE))

#ifndef __AZO_KEYWORD_C__
extern const char *azo_keywords[];
#endif

unsigned int azo_keyword_lookup (const unsigned char *text, unsigned int len);

/* Tests both token type and content */
unsigned int azo_token_is_keyword(const AZOToken *token, unsigned int keyword, const AZOSource *src);
/**
 * @brief Return the keyword code or AZO_KEYWORD_NONE if not a keyword
 * 
 * @param token The token to test
 * @param src The source file
 * @return The keyword code
 */
unsigned int azo_token_get_keyword(const AZOToken *token, const AZOSource *src);

void azo_print_keyword (unsigned int keyword, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
