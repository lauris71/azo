#define __AZO_ERRORS_C__

/*
 * A language implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016
 */

#include <stdio.h>
#include <stdarg.h>

#include <azo/compiler/compiler.h>
#include <azo/errors.h>

static const char *azo_errors[] = {
	"No error",
    /* Parser */
	"Unexpected EOF",
	"Syntax error",
	"End of block missing",
	"Missing semicolon",
	"Closing brace missing",
	"Invalid start of expression",
	"Invalid type expression (expected NAME[.NAME...] resolvable from globals)",
	"Too many errors",
    /* Resolver */
	"Variable already defined",
	"Variable is not defined"
};

static const char *azo_subsystems[] = {
	"Parser",
	"Resolver",
	"Optimizer",
	"Compiler"
};

static const char *azo_messages[] = {
	"Error",
	"Warning",
	"Info",
	"Debug"
};

const char *
azo_get_error_str(int errval)
{
	if ((errval < 0) || (errval >= AZO_NUM_ERRORS)) return "Unknown error";
	return azo_errors[errval];
}

const char *
azo_get_message_str(int msgval)
{
	if ((msgval < 0) || (msgval >= AZO_NUM_MESSAGE_TYPES)) return "Unknown message type";
	return azo_messages[msgval];
}

const char *
azo_get_subsystem_str(int subsystem)
{
	if ((subsystem < 0) || (subsystem >= AZO_NUM_SUBSYSTEMS)) return "Unknown subsystem";
	return azo_subsystems[subsystem];
}

void
azo_compiler_printf_message(AZOCompiler *comp, FILE *ofs, unsigned int type, unsigned int subsystem, unsigned int code_start, unsigned int code_end, const char *format, ...)
{
	unsigned int line;
	unsigned int first, last;
	if (azo_source_find_line_range (comp->src, code_start, code_end, &first, &last)) {
		line = first + 1;
	} else {
		line = comp->src->n_lines;
		first = last = comp->src->n_lines - 1;
	}
	fprintf(ofs, "%s:%s %s:%d ", azo_get_message_str(type), azo_get_subsystem_str(subsystem), comp->src->name->str, line);

    va_list args;
    va_start(args, format);
    vfprintf(ofs, format, args);
    va_end(args);

	if (last > (first + 2)) last = first + 2;
	azo_source_print_lines (comp->src, first, last + 1, ofs);
}

void
azo_compiler_print_error(AZOCompiler *comp, FILE *ofs, unsigned int subsystem, unsigned int code_start, unsigned int code_end, unsigned int errval)
{
	return azo_compiler_printf_message(comp, stderr, AZO_MESSAGE_ERROR, subsystem, code_start, code_end, "%s", azo_get_error_str(errval));
}

