#define __AZO_ERRORS_C__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016
 */

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

const char *
azo_get_error_str(int errval)
{
	if ((errval < 0) || (errval >= AZO_NUM_ERRORS)) return "Unknown error";
	return azo_errors[errval];
}

