#define __AZO_PROGRAM_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <stdlib.h>

#include <az/packed-value.h>
#include <azo/bytecode.h>

#include <azo/program.h>

void
azo_program_delete (AZOProgram *program)
{
	unsigned int i;
	if (program->tcode) free (program->tcode);
	for (i = 0; i < program->nvalues; i++) az_packed_value_clear (&program->values[i]);
	free (program->values);
	free (program);
}

void
azo_program_print_bytecode (AZOProgram *program)
{
	print_bytecode (program);
}

