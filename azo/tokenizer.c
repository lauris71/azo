#define __AZO_TOKEN_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arikkei/arikkei-strlib.h>

#include <az/extend.h>

#include <azo/operator.h>
#include <azo/tokenizer.h>

static void tokenizer_finalize (AZOTokenizerClass *klass, AZOTokenizer *tokenizer);

static unsigned int azo_tokenizer_type = 0;
static AZOTokenizerClass *azo_tokenizer_class = NULL;

unsigned int
azo_tokenizer_get_type (void)
{
	if (!azo_tokenizer_type) {
		az_register_type (&azo_tokenizer_type, (const unsigned char *) "AZOTokenizer", AZ_TYPE_BLOCK, sizeof (AZOTokenizerClass), sizeof (AZOTokenizer), AZ_FLAG_ZERO_MEMORY, 0, 0,
			NULL, NULL,
			(void (*) (const AZImplementation *, void *)) tokenizer_finalize);
		azo_tokenizer_class = (AZOTokenizerClass *) AZ_CLASS_FROM_TYPE(azo_tokenizer_type);
	}
	return azo_tokenizer_type;
}

static void
tokenizer_finalize (AZOTokenizerClass *klass, AZOTokenizer *tokenizer)
{
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
/* ctype functions require the argument to fit into unsigned char (or be EOF) */
#define IS_NUMBER_10(v) (((unsigned int) (v) < 256) && isdigit ((unsigned char) (v)))
#define IS_NUMBER_16(v) (((unsigned int) (v) < 256) && isxdigit ((unsigned char) (v)))
#define IS_SEPARATOR(v) (((v) <= ' ') || strchr (",;+-*/%=!<>&|^()[]{}~?:.", (v)) != NULL)
#define IS_ALPHA(v) ((((v) >= 'A') && ((v) <= 'Z')) || (((v) >= 'a') && ((v) <= 'z')) || ((v) == '_'))
#define IS_LINE_END(v) (((v) == 10) || ((v) == 13))

/* Cursor access macros
 * These assume the current text to be in cdata/csize
 * Reading past the end of input returns NUL */
#define C_HAS_CHAR(p) ((p) < csize)
#define C_PEEK(p) (C_HAS_CHAR (p) ? cdata[p] : 0)
#define C_IS(p, c) (C_PEEK (p) == (c))
#define C_IS_ANY_OF(p, s) (C_PEEK (p) && (strchr (s, C_PEEK (p)) != NULL))
#define C_IS_NUMBER_2(p) IS_NUMBER_2 (C_PEEK (p))
#define C_IS_NUMBER_10(p) IS_NUMBER_10 (C_PEEK (p))
#define C_IS_NUMBER_16(p) IS_NUMBER_16 (C_PEEK (p))
#define C_IS_SEPARATOR(p) IS_SEPARATOR (C_PEEK (p))
#define C_IS_LINE_END(p) IS_LINE_END (C_PEEK (p))
#define C_IS_TEXT(p, t) (c_match_text (cdata, csize, p, t) > 0)
#define C_MATCH_TEXT(p, t) c_match_text (cdata, csize, p, t)

/* Check whether the text at cpos matches the given (NUL-terminated) string
 * Returns the length of the match (0 if the text does not match) */
static unsigned int
c_match_text (const unsigned char *cdata, unsigned int csize, unsigned int cpos, const char *text)
{
	unsigned int p = 0;
	while (text[p] && ((cpos + p) < csize) && (cdata[cpos + p] == text[p])) p += 1;
	return text[p] ? 0 : p;
}

static unsigned int get_token (AZOTokenizer *tokenizer, unsigned int cpos, AZOToken *token);

void
azo_tokenizer_setup (AZOTokenizer *tokenizer, const uint8_t *cdata, unsigned int csize)
{
	az_instance_init_by_type (tokenizer, AZO_TYPE_TOKENIZER);
	tokenizer->cdata = cdata;
	tokenizer->csize = csize;
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
	while ((cpos < tokenizer->csize) && (tokenizer->cdata[cpos] <= ' ')) {
		cpos += 1;
	}
	return cpos < tokenizer->csize;
}

unsigned int
azo_tokenizer_get_next_token (AZOTokenizer *tokenizer, AZOToken *current)
{
	unsigned int cpos = current->end;
	while (cpos < tokenizer->csize) {
		while ((cpos < tokenizer->csize) && (tokenizer->cdata[cpos] <= ' ')) {
			cpos += 1;
		}
		if (cpos >= tokenizer->csize) break;
		current->start = cpos;
		cpos += get_token (tokenizer, cpos, current);
		switch (current->type) {
			case AZO_TOKEN_NONE:
			case AZO_TOKEN_EMPTY:
			case AZO_TOKEN_COMMENT:
				continue;
			case AZO_TOKEN_INVALID:
				return 0;
			default:
				return 1;
		}
	}
	*current = (AZOToken) {cpos, cpos, AZO_TOKEN_EOF};
	return 0;
}

unsigned int
azo_tokenizer_skip_line (AZOTokenizer *tokenizer, AZOToken *current)
{
	current->start = current->end;
	if (current->start >= tokenizer->csize) {
		current->type = AZO_TOKEN_EOF;
		return 0;
	}
	current->type = AZO_TOKEN_NONE;
	while (current->end < tokenizer->csize) {
		if (tokenizer->cdata[current->end] == '\n') {
			break;
		}
		current->end += 1;
	}
	return 1;
}

static unsigned int
get_number (AZOTokenizer *tokenizer, unsigned int cpos, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->cdata + cpos;
	unsigned int csize = tokenizer->csize - cpos;
	unsigned int p = 0;
	unsigned int type = AZO_TOKEN_INTEGER;
	if (C_IS (p, '0')) {
		if (C_IS_ANY_OF (p + 1, "bB")) {
			/* Binary */
			p += 2;
			while (C_IS_NUMBER_2 (p)) p += 1;
			/* At least one digit is required */
			type = (p > 2) ? AZO_TOKEN_INTEGER_BIN : AZO_TOKEN_INVALID;
		} else if (C_IS_ANY_OF (p + 1, "xX")) {
			/* Hex */
			p += 2;
			while (C_IS_NUMBER_16 (p)) p += 1;
			/* At least one digit is required */
			type = (p > 2) ? AZO_TOKEN_INTEGER_HEX : AZO_TOKEN_INVALID;
		}
	}
	if (type == AZO_TOKEN_INTEGER) {
		/* Decimal */
		while (C_IS_NUMBER_10 (p)) p += 1;
		if (C_IS (p, '.')) {
			/* Real */
			type = AZO_TOKEN_FLOATING_POINT;
			p += 1;
			while (C_IS_NUMBER_10 (p)) p += 1;
		}
		if (C_IS_ANY_OF (p, "eE")) {
			/* Real with exponent */
			p += 1;
			if (C_IS_ANY_OF (p, "+-")) p += 1;
			if (C_IS_NUMBER_10 (p)) {
				type = AZO_TOKEN_FLOATING_POINT;
				while (C_IS_NUMBER_10 (p)) p += 1;
			} else {
				/* Exponent requires at least one digit */
				type = AZO_TOKEN_INVALID;
			}
		}
	}
	/* Qualifiers */
	if ((type == AZO_TOKEN_INTEGER) || (type == AZO_TOKEN_FLOATING_POINT)) {
		/* Possible qualifiers: f, i, fi */
		if (C_IS_ANY_OF (p, "fF")) {
			p += 1;
			type = AZO_TOKEN_FLOATING_POINT;
		}
		if (C_IS_ANY_OF (p, "iI")) {
			p += 1;
			type = AZO_TOKEN_FLOATING_POINT;
		}
	}
	if ((type == AZO_TOKEN_INTEGER) || (type == AZO_TOKEN_INTEGER_BIN) || (type == AZO_TOKEN_INTEGER_HEX)) {
		/* Possible qualifiers: u, l, ul, lu (ll is invalid, integers are either 32 or 64 bit) */
		if (C_IS_ANY_OF (p, "uU")) {
			p += 1;
			if (C_IS_ANY_OF (p, "lL")) p += 1;
		} else if (C_IS_ANY_OF (p, "lL")) {
			p += 1;
			if (C_IS_ANY_OF (p, "uU")) p += 1;
		}
	}
	/* Numbers have to be terminated by a separator */
	if (C_HAS_CHAR (p) && !C_IS_SEPARATOR (p)) {
		type = AZO_TOKEN_INVALID;
	}
	token->type = type;
	token->end = token->start + p;
	return p;
}

static unsigned int
get_cpp_comment (AZOTokenizer *tokenizer, unsigned int cpos, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->cdata + cpos;
	unsigned int csize = tokenizer->csize - cpos;
	unsigned int p = 2;
	/* C++ style comment until the end of line */
	while (C_HAS_CHAR (p) && !C_IS_LINE_END (p)) p += 1;
	token->type = AZO_TOKEN_COMMENT;
	token->end = token->start + p;
	return p;
}

static unsigned int
get_c_comment (AZOTokenizer *tokenizer, unsigned int cpos, AZOToken *token)
{
	const unsigned char *cdata = tokenizer->cdata + cpos;
	unsigned int csize = tokenizer->csize - cpos;
	unsigned int p = 2;
	/* C style comment (possibly multi-line) */
	while (C_HAS_CHAR (p)) {
		if (C_IS_TEXT (p, "*/")) {
			p += 2;
			token->type = AZO_TOKEN_COMMENT;
			token->end = token->start + p;
			return p;
		} else {
			p += 1;
		}
	}
	/* Unterminated comment - consume the rest of input */
	token->type = AZO_TOKEN_INVALID;
	token->end = token->start + csize;
	return csize;
}

static unsigned int
get_token (AZOTokenizer *tokenizer, unsigned int cpos, AZOToken *token)
{
	int unival;
	unsigned int i;

	const unsigned char *cdata = tokenizer->cdata + cpos;
	unsigned int csize = tokenizer->csize - cpos;
	unsigned int p = 0;
	/* Comments */
	if (C_IS_TEXT (p, "//")) {
		return get_cpp_comment (tokenizer, cpos, token);
	}
	if (C_IS_TEXT (p, "/*")) {
		return get_c_comment (tokenizer, cpos, token);
	}

	/* Number starting with the decimal point (has to be tried before operators or . would match the dot operator) */
	if (C_IS (p, '.') && C_IS_NUMBER_10 (p + 1)) {
		return get_number (tokenizer, cpos, token);
	}

	/* Operators */
	for (i = 0; i < AZO_NUM_OPERATORS; i++) {
		unsigned int len = C_MATCH_TEXT (p, azo_operators[i].text);
		if (len) {
			p += len;
			token->type = AZO_TOKEN_OPERATOR | azo_operators[i].type;
			token->end = token->start + p;
			return p;
		}
	}

	/* Standard tokens */
	for (i = 0; i < NUM_DESCRIPTIONS; i++) {
		unsigned int len = C_MATCH_TEXT (p, descriptions[i].text);
		if (len) {
			p += len;
			token->type = descriptions[i].type;
			token->end = token->start + p;
			return p;
		}
	}

	/* Numbers */
	if (C_IS_NUMBER_10 (p)) {
		return get_number (tokenizer, cpos, token);
	}
	/* Text */
	if (C_IS (p, '\"')) {
		p += 1;
		while (C_HAS_CHAR (p) && !C_IS (p, '\"')) {
			if (C_PEEK (p) < ' ') {
				token->type = AZO_TOKEN_INVALID;
				token->end = token->start + p;
				return p;
			}
			if (C_IS (p, '\\')) {
				p += 1;
				if (!C_HAS_CHAR (p)) break;
			}
			p += 1;
		}
		if (!C_HAS_CHAR (p)) {
			token->type = AZO_TOKEN_INVALID;
			token->end = token->start + p;
			return p;
		}
		token->type = AZO_TOKEN_TEXT;
		token->end = token->start + p + 1;
		return p + 1;
	}

	const unsigned char *u = cdata + p;
	unival = arikkei_utf8_get_unicode (&u, csize - p);
	if (unival < 0) {
		/* Invalid unicode value */
		p += 1;
		token->type = AZO_TOKEN_INVALID;
		token->end = token->start + p;
		return p;
	}
	p = (unsigned int) (u - cdata);
	/* Word */
	if (IS_ALPHA (unival)) {
		const unsigned char *r;
		r = cdata + p;
		while (C_HAS_CHAR (p) && (IS_ALPHA (unival) || IS_NUMBER_10 (unival))) {
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
		fprintf (ofs, "%c", tokenizer->cdata[i]);
	}
}
