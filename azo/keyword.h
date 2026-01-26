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
	/* CLASS, LIST */
	AZO_KEYWORD_NEW,
	/* CONDITION, TRUE_STATEMENT, FALSE_STATEMENT */
	AZO_KEYWORD_IF,
	AZO_KEYWORD_ELSE,
	/* [VALUE.] FUNCTION, [TYPE], LIST, STATEMENT */
	AZO_KEYWORD_FUNCTION,
	/* RETURN [VALUE] */
	AZO_KEYWORD_RETURN,
	/* REFERENCE IS REFERENCE */
	AZO_KEYWORD_IS,
	/* REFERENCE IMPLEMENTS REFERENCE */
	AZO_KEYWORD_IMPLEMENTS,

	/* DEBUG */
	AZO_KEYWORD_DEBUG,

	AZO_NUM_KEYWORDS
};

#ifndef __AZO_KEYWORD_C__
extern const char *azo_keywords[];
#endif

unsigned int azo_keyword_lookup (const unsigned char *text, unsigned int len);

/* Tests both token type and content */
unsigned int azo_token_is_keyword (const AZOSource *src, const AZOToken *token, unsigned int keyword);

void azo_print_keyword (unsigned int keyword, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
