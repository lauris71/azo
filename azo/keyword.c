#define __AZO_KEYWORD_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <stdlib.h>
#include <string.h>

#include <azo/keyword.h>

const char *azo_keywords[] = {
	NULL,
	"null",
	"void",
	"static",
	"const",
	"final",
	"true",
	"false",
	"this",
	"for",
	"while",
	"do",
	"new",
	"if",
	"else",
	"return",
	"is",
	"implements",
	"as",
	"boolean",
	"int8",
	"uint8",
	"int16",
	"uint16",
	"int32",
	"uint32",
	"int64",
	"uint64",
	"float",
	"double",
	"complex",
	"pointer",
	"debug",
	"break",
	"continue",
	"exact",
	"rounded"
#ifdef HAS_FUNCTION_KEYWORD
	/* LEGACY - kept for old scripts, new code uses lambdas (=>) */
	, "function"
#endif
};

unsigned int
azo_keyword_lookup (const unsigned char *text, unsigned int len)
{
	unsigned int i;
	for (i = 1; i < AZO_NUM_KEYWORDS; i++) {
		unsigned int kwlen = (unsigned int) strlen (azo_keywords[i]);
		if (kwlen == len) {
			if (!strncmp ((const char *) text, azo_keywords[i], len)) return i;
		}
	}
	return AZO_KEYWORD_NONE;
}

unsigned int
azo_token_is_keyword (const AZOToken *token, unsigned int keyword, const AZOSource *src)
{
	unsigned int kwlen;
	if (token->type != AZO_TOKEN_WORD) return 0;
	kwlen = (unsigned int) strlen (azo_keywords[keyword]);
	if (kwlen == token->end - token->start) {
		if (!strncmp ((const char *) src->cdata + token->start, azo_keywords[keyword], kwlen)) return 1;
	}
	return 0;
}

unsigned int
azo_token_get_keyword(const AZOToken *token, const AZOSource *src)
{
	if (token->type != AZO_TOKEN_WORD) return AZO_KEYWORD_NONE;
	for (unsigned int keyword = 1; keyword < AZO_NUM_KEYWORDS; keyword++) {
		unsigned int kwlen = (unsigned int) strlen (azo_keywords[keyword]);
		if (kwlen == token->end - token->start) {
			if (!strncmp ((const char *) src->cdata + token->start, azo_keywords[keyword], kwlen)) return keyword;
		}
	}
	return AZO_KEYWORD_NONE;
}

void
azo_print_keyword (unsigned int keyword, FILE *ofs)
{
	fprintf (ofs, "%s", azo_keywords[keyword]);
}
