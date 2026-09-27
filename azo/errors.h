#ifndef __AZO_ERRORS_H__
#define __AZO_ERRORS_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
enum {
	AZO_ERROR_NONE,
    /* Parser */
	AZO_PARSER_ERROR_UNEXPECTED_EOF,
	AZO_PARSER_ERROR_SYNTAX,
	AZO_PARSER_ERROR_END_OF_BLOCK_MISSING,
	AZO_PARSER_ERROR_SEMICOLON_MISSING,
	AZO_PARSER_ERROR_CLOSING_PARENTHESIS_MISSING,
	AZO_PARSER_ERROR_INVALID_START_OF_EXPRESSION,
	AZO_PARSER_ERROR_INVALID_TYPE_EXPRESSION,
	AZO_PARSER_ERROR_TOO_MANY_ERRORS,
    /* Resolver */
    AZO_VARIABLE_DEFINED,
    AZO_VARIABLE_NOT_DEFINED,

    AZO_NUM_ERRORS
};

const char *azo_get_error_str(int errval);

#ifdef __cplusplus
}
#endif

#endif

