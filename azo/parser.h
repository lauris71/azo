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
typedef struct _AZOParserError AZOParserError;

#define AZO_TYPE_PARSER azo_parser_get_type ()

#include <azo/node.h>
#include <azo/source.h>
#include <azo/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parser error codes */
enum {
	AZO_PARSER_ERROR_NONE,
	AZO_PARSER_ERROR_UNEXPECTED_EOF,
	AZO_PARSER_ERROR_SYNTAX,
	AZO_PARSER_ERROR_END_OF_BLOCK_MISSING,
	AZO_PARSER_ERROR_SEMICOLON_MISSING,
	AZO_PARSER_ERROR_CLOSING_PARENTHESIS_MISSING,
	AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION,
	AZO_PARSER_ERROR_TOO_MANY_ERRORS,
	AZO_PARSER_NUM_ERRORS
};

/* The maximum number of errors logged into the parser error list */
#define AZO_PARSER_MAX_ERRORS 20

struct _AZOParserError {
	/* Error code (AZO_PARSER_ERROR_*) */
	unsigned int code;
	/* The offending token */
	AZOToken token;
	/* Error message (empty if none) */
	char message[64];
};

struct _AZOParser {
	AZOSource *src;

	AZOTokenizer tokenizer;

	/**
	 * @brief Current expression, sub-expressions are added as it's children
	 * 
	 */
	AZONode *current;

	/* Errors encountered during parsing (oldest first) */
	unsigned int n_errors;
	AZOParserError errors[AZO_PARSER_MAX_ERRORS];
};

struct _AZOParserClass {
	AZClass klass;
};

unsigned int azo_parser_get_type (void);

void azo_parser_setup (AZOParser *parser, AZOSource *src);
void azo_parser_release (AZOParser *parser);

AZONode *azo_parser_parse (AZOParser *parser);

/**
 * @brief Get the default message for a parser error code
 * 
 * @param code the error code (AZO_PARSER_ERROR_*)
 * @return the message string
 */
const char *azo_parser_error_get_message (unsigned int code);

#ifdef __cplusplus
}
#endif

#endif
