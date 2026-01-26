#ifndef __AZO_PROGRAM_H__
#define __AZO_PROGRAM_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <az/value.h>

#include <azo/context.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOProgram AZOProgram;

struct _AZOProgram {
	AZOContext *ctx;
	/* Typecode */
	unsigned char *tcode;
	unsigned int tcode_length;
	/* Immediate values */
	unsigned int nvalues;
	AZPackedValue *values;
};

void azo_program_delete (AZOProgram *program);

void azo_program_print_bytecode (AZOProgram *program);

#ifdef __cplusplus
}
#endif

#endif
