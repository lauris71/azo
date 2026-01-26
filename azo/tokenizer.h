#ifndef __AZO_TOKEN_H__
#define __AZO_TOKEN_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016
 */

typedef struct _AZOTokenizer AZOTokenizer;
typedef struct _AZOTokenizerClass AZOTokenizerClass;

#define AZO_TYPE_TOKENIZER azo_tokenizer_get_type ()

#include <stdio.h>

#include <az/class.h>

#include <azo/operator.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOToken AZOToken;

/* Token types */
#define AZO_TOKEN_TYPE_MASK 0xffffff00
#define AZO_TOKEN_SUBTYPE_MASK 0xff
/* Special */
/* These should not appear in tokenized list */
#define AZO_TOKEN_INVALID 0
#define AZO_TOKEN_NONE 1
#define AZO_TOKEN_EMPTY 2
#define AZO_TOKEN_COMMENT 3
#define AZO_TOKEN_EOF 4
/* Actual tokens */
#define AZO_TOKEN_NUMBER (1UL << 8)
#define AZO_TOKEN_TEXT (2UL << 8)
#define AZO_TOKEN_WORD (3UL << 8)
#define AZO_TOKEN_SEMICOLON (4UL << 8)
#define AZO_TOKEN_OPERATOR (5UL << 8)
#define AZO_TOKEN_BRACKET (6UL << 8)

/* Number subtypes */
#define AZO_TOKEN_INTEGER (AZO_TOKEN_NUMBER | 0)
#define AZO_TOKEN_INTEGER_HEX (AZO_TOKEN_NUMBER | 1)
#define AZO_TOKEN_INTEGER_BIN (AZO_TOKEN_NUMBER | 2)
#define AZO_TOKEN_REAL (AZO_TOKEN_NUMBER | 3)
#define AZO_TOKEN_IMAGINARY (AZO_TOKEN_NUMBER | 4)

/* Bracket subtypes */
#define AZO_TOKEN_LEFT_PARENTHESIS (AZO_TOKEN_BRACKET | 0)
#define AZO_TOKEN_RIGHT_PARENTHESIS (AZO_TOKEN_BRACKET | 1)
#define AZO_TOKEN_LEFT_BRACKET (AZO_TOKEN_BRACKET | 2)
#define AZO_TOKEN_RIGHT_BRACKET (AZO_TOKEN_BRACKET | 3)
#define AZO_TOKEN_LEFT_BRACE (AZO_TOKEN_BRACKET | 4)
#define AZO_TOKEN_RIGHT_BRACE (AZO_TOKEN_BRACKET | 5)

/* Relevant operators */
#define AZO_TOKEN_ASSIGN (AZO_TOKEN_OPERATOR | AZO_OPERATOR_ASSIGN)
#define AZO_TOKEN_COMMA (AZO_TOKEN_OPERATOR | AZO_OPERATOR_COMMA)

#define AZO_TOKEN_IS_NUMBER(t) (((t)->type & AZO_TOKEN_TYPE_MASK) == AZO_TOKEN_NUMBER)
#define AZO_TOKEN_IS_INTEGER(t) (((t)->type == AZO_TOKEN_INTEGER) || ((t)->type == AZO_TOKEN_INTEGER_HEX) || ((t)->type == AZO_TOKEN_INTEGER_BIN))
#define AZO_TOKEN_IS_OPERATOR(t) (((t)->type & AZO_TOKEN_TYPE_MASK) == AZO_TOKEN_OPERATOR)
#define AZO_TOKEN_OPERATOR_CODE(t) ((t)->type & AZO_TOKEN_SUBTYPE_MASK)

struct _AZOTokenizer {
	AZOSource *src;
	unsigned int cpos;
};

struct _AZOTokenizerClass {
	AZClass klass;
};

unsigned int azo_tokenizer_get_type (void);

struct _AZOToken {
	unsigned int start;
	unsigned int end;
	unsigned int type;
};

void azo_tokenizer_setup (AZOTokenizer *tokenizer, const unsigned char *cdata, unsigned int csize);
void azo_tokenizer_release (AZOTokenizer *tokenizer);

unsigned int azo_tokenizer_is_eof (AZOTokenizer *tokenizer, const AZOToken *current);
unsigned int azo_tokenizer_has_next_token (AZOTokenizer *tokenizer, const AZOToken *current);
/* Return true on success */
unsigned int azo_tokenizer_get_next_token (AZOTokenizer *tokenizer, AZOToken *current);

void azo_tokenizer_print_token (AZOTokenizer *tokenizer, const AZOToken *token, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
