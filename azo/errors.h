#ifndef __AZO_ERRORS_H__
#define __AZO_ERRORS_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016
 */

 #include <stdint.h>
 #include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOCompiler AZOCompiler;

enum AZOSubSystem {
	AZO_SUBSYSTEM_PARSER,
	AZO_SUBSYSTEM_RESOLVER,
	AZO_SUBSYSTEM_OPTIMIZER,
	AZO_SUBSYSTEM_COMPILER,
	AZO_NUM_SUBSYSTEMS
};

enum AZOMessageType {
	AZO_MESSAGE_ERROR,
	AZO_MESSAGE_WARNING,
	AZO_MESSAGE_INFO,
	AZO_MESSAGE_DEBUG,
	AZO_NUM_MESSAGE_TYPES
};

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

void azo_compiler_printf_message(AZOCompiler *comp, FILE *ofs, unsigned int type, unsigned int subsystem, unsigned int code_start, unsigned int code_end, const char *format, ...);
void azo_compiler_print_error(AZOCompiler *comp, FILE *ofs, unsigned int subsystem, unsigned int code_start, unsigned int code_end, unsigned int errval);

#ifdef __cplusplus
}
#endif

#endif

