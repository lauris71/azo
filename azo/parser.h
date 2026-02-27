#ifndef __AZO_PARSER_H__
#define __AZO_PARSER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOParser AZOParser;
typedef struct _AZOParserClass AZOParserClass;
typedef struct _AZOParserScope AZOParserScope;

#define AZO_TYPE_PARSER azo_parser_get_type ()

#include <azo/expression.h>
#include <azo/source.h>
#include <azo/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOParser {
	AZOSource *src;

	AZOTokenizer tokenizer;

	/**
	 * @brief Current expression, sub-expressions are added as it's children
	 * 
	 */
	AZOExpression *current;
};

struct _AZOParserClass {
	AZClass klass;
};

unsigned int azo_parser_get_type (void);

void azo_parser_setup (AZOParser *parser, AZOSource *src);
void azo_parser_release (AZOParser *parser);

AZOExpression *azo_parser_parse (AZOParser *parser);

#ifdef __cplusplus
}
#endif

#endif
