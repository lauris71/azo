#define __AZO_TOKEN_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arikkei/arikkei-strlib.h>

#include <az/extend.h>

#include <azo/operator.h>
#include <azo/source.h>
#include <azo/tokenizer.h>

static void tokenizer_finalize (AZOTokenizerClass *klass, AZOTokenizer *tokenizer);

static unsigned int azo_tokenizer_type = 0;
static AZOTokenizerClass *azo_tokenizer_class = NULL;

unsigned int
azo_tokenizer_get_type (void)
{
	if (!azo_tokenizer_type) {
		az_register_type (&azo_tokenizer_type, (const unsigned char *) "AZOTokenizer", AZ_TYPE_BLOCK, sizeof (AZOTokenizerClass), sizeof (AZOTokenizer), AZ_FLAG_ZERO_MEMORY,
			NULL, NULL,
			(void (*) (const AZImplementation *, void *)) tokenizer_finalize);
		azo_tokenizer_class = (AZOTokenizerClass *) AZ_CLASS_FROM_TYPE(azo_tokenizer_type);
	}
	return azo_tokenizer_type;
}

static void
tokenizer_finalize (AZOTokenizerClass *klass, AZOTokenizer *tokenizer)
{
	azo_source_unref(tokenizer->src);
}

struct AZOTokenDescription {
	unsigned int type;
	const char *text;
};

/* Operator descriptions */
struct AZOTokenDescription descriptions[] = {
	/* Non-operator types */
	{ AZO_TOKEN_SEMICOLON, ";" },
	/* Brackets */
	{ AZO_TOKEN_LEFT_PARENTHESIS, "(" },
	{ AZO_TOKEN_RIGHT_PARENTHESIS, ")" },
	{ AZO_TOKEN_LEFT_BRACKET, "[" },
	{ AZO_TOKEN_RIGHT_BRACKET, "]" },
	{ AZO_TOKEN_LEFT_BRACE, "{" },
	{ AZO_TOKEN_RIGHT_BRACE, "}" }
};

#define NUM_DESCRIPTIONS (sizeof (descriptions) / sizeof (descriptions[0]))

#define IS_NUMBER_2(v) (((v) == '0') || ((v) == '1'))
#define IS_NUMBER_10(v) (((v) >= '0') && ((v) <= '9'))
#define IS_NUMBER_16(v) ((((v) >= '0') && ((v) == '1')) || (((v) >= 'A') && ((v) <= 'F')) || (((v) >= 'a') && ((v) <= 'f')))
/* fixme: All whitespace types */
#define IS_WS(v) (((v) < ' ') || (v == 160))
#define IS_SEPARATOR(v) (((v) <= ' ') || strchr (",;+-/*<>&|^()[]{}", (v)) != NULL)
#define IS_ALPHA(v) ((((v) >= 'A') && ((v) <= 'Z')) || (((v) >= 'a') && ((v) <= 'z')) || ((v) == '_'))
#define IS_LINE_END(v) (((v) == 10) || ((v) == 13))

static void tokenizer_ensure_lines (AZOTokenizer *tokenizer, unsigned int req);
static void tokenizer_ensure_tokens (AZOTokenizer *tokenizer, unsigned int req);
static unsigned int get_token (AZOTokenizer *tokenizer, AZOToken *token);

void
azo_tokenizer_setup (AZOTokenizer *tokenizer, const unsigned char *cdata, unsigned int csize)
{
	az_instance_init_by_type (tokenizer, AZO_TYPE_TOKENIZER);
	tokenizer->src = azo_source_new_static(cdata, csize);
}

void
azo_tokenizer_release (AZOTokenizer *tokenizer)
{
	az_instance_finalize_by_type (tokenizer, AZO_TYPE_TOKENIZER);
}

unsigned int
azo_tokenizer_is_eof (AZOTokenizer *tokenizer, const AZOToken *current)
{
	return !azo_tokenizer_has_next_token(tokenizer, current);
}

unsigned int
azo_tokenizer_has_next_token (AZOTokenizer *tokenizer, const AZOToken *current)
{
	unsigned int cpos = current->end;
	while ((cpos < tokenizer->src->csize) && (tokenizer->src->cdata[cpos] <= ' ')) {
		cpos += 1;
	}
	return cpos < tokenizer->src->csize;
}

unsigned int
azo_tokenizer_get_next_token (AZOTokenizer *tokenizer, AZOToken *current)
{
	while (tokenizer->cpos < tokenizer->src->csize) {
		unsigned int cpos = tokenizer->cpos;
		if (cpos != current->end) {
			fprintf(stderr, ".");
		}
		while ((cpos < tokenizer->src->csize) && (tokenizer->src->cdata[cpos] <= ' ')) {
			cpos += 1;
		}
		tokenizer->cpos = cpos;
		if (tokenizer->cpos >= tokenizer->src->csize) break;
		current->start = tokenizer->cpos;
		tokenizer->cpos += get_token (tokenizer, current);
		switch (current->type) {
			case AZO_TOKEN_INVALID:
				return 0;
				break;
			case AZO_TOKEN_NONE:
			case AZO_TOKEN_EMPTY:
			case AZO_TOKEN_COMMENT:
				break;
			default:
				return 1;
				break;
		}
	}
	*current = (AZOToken) {tokenizer->cpos, tokenizer->cpos, AZO_TOKEN_EOF};
	return 0;
}

static unsigned int
get_number (AZOTokenizer *tokenizer, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->src->cdata + tokenizer->cpos;
	unsigned int csize = tokenizer->src->csize - tokenizer->cpos;
	unsigned int p = 0;
	unsigned int type = AZO_TOKEN_INTEGER;
	if ((cdata[p] == '0') && (csize >= 3)) {
		if ((cdata[p + 1] == 'b') || (cdata[p + 1] == 'B')) {
			/* Binary */
			p += 2;
			while ((p < csize) && IS_NUMBER_2 (cdata[p])) p += 1;
			type = AZO_TOKEN_INTEGER_BIN;
		} else if ((cdata[p + 1] == 'x') || (cdata[p + 1] == 'X')) {
			/* Hex */
			p += 2;
			while ((p < csize) && IS_NUMBER_16 (cdata[p])) p += 1;
			type = AZO_TOKEN_INTEGER_HEX;
		}
	}
	if (type == AZO_TOKEN_INTEGER) {
		/* Decimal */
		while ((p < csize) && IS_NUMBER_10 (cdata[p])) p += 1;
		if ((p < csize) && (cdata[p] == '.')) {
			/* Real */
			type = AZO_TOKEN_REAL;
			p += 1;
			while ((p < csize) && IS_NUMBER_10 (cdata[p])) p += 1;
		}
		if ((p < csize) && ((cdata[p] == 'e') || (cdata[p] == 'E'))) {
			/* Real with exponent */
			p += 1;
			while ((p < csize) && IS_NUMBER_10 (cdata[p])) p += 1;
		}
	}
	/* Qualifiers */
	if ((type == AZO_TOKEN_INTEGER) || (type == AZO_TOKEN_REAL)) {
		/* Possible qualifiers: f, i, fi */
		if ((p < csize) && ((cdata[p] == 'f') || (cdata[p] == 'F'))) {
			p += 1;
			type = AZO_TOKEN_REAL;
		}
		if ((p < csize) && ((cdata[p] == 'i') || (cdata[p] == 'I'))) {
			p += 1;
			type = AZO_TOKEN_IMAGINARY;
		}
	}
	if ((type == AZO_TOKEN_INTEGER) || (type == AZO_TOKEN_INTEGER_BIN) || (type == AZO_TOKEN_INTEGER_HEX)) {
		/* Possible qualifiers: u, l, ul */
		if ((p < csize) && ((cdata[p] == 'u') || (cdata[p] == 'U'))) p += 1;
		if ((p < csize) && ((cdata[p] == 'l') || (cdata[p] == 'L'))) p += 1;
	}
	if ((p < csize) && !IS_SEPARATOR(cdata[p])) {
		type = AZO_TOKEN_INVALID;
	}
	token->type = type;
	token->end = token->start + p;
	tokenizer->cpos += p;
	return 0;
}

static unsigned int
get_cpp_comment (AZOTokenizer *tokenizer, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->src->cdata + tokenizer->cpos;
	unsigned int csize = tokenizer->src->csize - tokenizer->cpos;
	unsigned int p = 2;
	/* C++ style comment until the end of line */
	while ((p < csize) && !IS_LINE_END (cdata[p])) p += 1;
	token->type = AZO_TOKEN_COMMENT;
	token->end = token->start + p;
	tokenizer->cpos += p;
	return 0;
}

static unsigned int
get_c_comment (AZOTokenizer *tokenizer, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->src->cdata + tokenizer->cpos;
	unsigned int csize = tokenizer->src->csize - tokenizer->cpos;
	unsigned int p = 2;
	/* C style comment (possibly multi-line) */
	while (p < (csize - 1)) {
		if (!strncmp ((const char *) cdata + p, "*/", 2)) {
			p += 2;
			token->type = AZO_TOKEN_COMMENT;
			token->end = token->start + p;
			tokenizer->cpos += p;
			return 0;
		} else {
			p += 1;
		}
	}
	token->type = AZO_TOKEN_INVALID;
	token->end = token->start + p;
	tokenizer->cpos += p;
	return 0;
}

static unsigned int
get_token (AZOTokenizer *tokenizer, AZOToken *token)
{
	unsigned int unival;
	unsigned int i;

	const unsigned char *cdata = tokenizer->src->cdata + tokenizer->cpos;
	unsigned int csize = tokenizer->src->csize - tokenizer->cpos;
	unsigned int p = 0;
	/* Comments */
	if ((csize >= 2) && !strncmp ((const char *) cdata + p, "//", 2)) {
		return get_cpp_comment(tokenizer, token);
	}
	if ((csize >= 2) && !strncmp ((const char *) cdata + p, "/*", 2)) {
		return get_c_comment(tokenizer, token);
	}

	/* Operators */
	for (i = 0; i < AZO_NUM_OPERATORS; i++) {
		unsigned int j = 0;
		while ((cdata[p + j] == azo_operators[i].text[j]) && azo_operators[i].text[j] && ((p + j) < csize)) j += 1;
		if (!azo_operators[i].text[j]) {
			p += j;
			token->type = AZO_TOKEN_OPERATOR | azo_operators[i].type;
			token->end = token->start + p;
			return p;
		}
	}

	/* Standard tokens */
	for (i = 0; i < NUM_DESCRIPTIONS; i++) {
		int j = 0;
		while ((cdata[p + j] == descriptions[i].text[j]) && descriptions[i].text[j] && ((p + j) < csize)) j += 1;
		if (!descriptions[i].text[j]) {
			p += j;
			token->type = descriptions[i].type;
			token->end = token->start + p;
			return p;
		}
	}

	/* Numbers */
	if (IS_NUMBER_10 (cdata[p])) {
		return get_number (tokenizer, token);
	}
	/* Text */
	if (cdata[p] == '\"') {
		unsigned int start = p;
		p += 1;
		while ((p < csize) && (cdata[p] != '\"')) {
			if (cdata[p] < ' ') {
				token->type = AZO_TOKEN_INVALID;
				token->end = token->start + p;
				return p;
			}
			if (cdata[p] == '\\') {
				p += 1;
				if (p >= csize) break;
			}
			p += 1;
		}
		if (p >= csize) {
			token->type = AZO_TOKEN_INVALID;
			token->end = token->start + p;
			return p;
		}
		token->type = AZO_TOKEN_TEXT;
		token->start = start + (unsigned int) (cdata - tokenizer->src->cdata);
		token->end = token->start + p + 1 - start;
		return p + 1;
	}

	const unsigned char *u = cdata + p;
	unival = arikkei_utf8_get_unicode (&u, csize - p);
	if (unival < 0) {
		/* Invalid unicode value */
		p += 1;
		token->type = AZO_TOKEN_INVALID;
		return p;
	}
	p = (unsigned int) (u - cdata);
	/* Word */
	if (IS_ALPHA (unival)) {
		const unsigned char *r;
		r = cdata + p;
		while ((p < csize) && (IS_ALPHA (unival) || IS_NUMBER_10 (unival))) {
			p = (unsigned int) (r - cdata);
			unival = arikkei_utf8_get_unicode (&r, csize - p);
		}
		token->type = AZO_TOKEN_WORD;
		token->end = token->start + p;
		return p;
	}
	token->type = AZO_TOKEN_INVALID;
	token->end = token->start + p;
	return p;
}

void
azo_tokenizer_print_token (AZOTokenizer *tokenizer, const AZOToken *token, FILE *ofs)
{
	unsigned int i;
	for (i = token->start; i < token->end; i++) {
		fprintf (ofs, "%c", tokenizer->src->cdata[i]);
	}
}
