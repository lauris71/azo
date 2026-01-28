#ifndef __AZO_INTERPRETER_H__
#define __AZO_INTERPRETER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

typedef struct _AZOInterpreter AZOInterpreter;

#include <stdio.h>

#include <az/packed-value.h>

#include <azo/context.h>
#include <azo/exception.h>
#include <azo/program.h>
#include <azo/stack.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _REGISTER_THIS 0

struct _AZOInterpreter {
	AZOContext *ctx;
	AZOProgram *prog;

	/* Current stack frame start */
	unsigned int n_frames;
	unsigned int size_frames;
	unsigned int *frames;
	AZOStack stack;
	AZOException exc;
	/* Register */
	AZPackedValue64 vals[4];
};


AZOInterpreter *azo_interpreter_new (AZOContext *ctx);
void interpreter_delete (AZOInterpreter *intr);

void azo_intepreter_push_instance (AZOInterpreter *intr, const AZImplementation *impl, void *inst);
void azo_intepreter_push_value (AZOInterpreter *intr, const AZImplementation *impl, const AZValue *val);
void azo_intepreter_push_values (AZOInterpreter *intr, const AZImplementation **impls, const AZValue **vals, unsigned int n_vals);
/* Create new frame containing n_elements elements at the top of stack */
void azo_interpreter_push_frame (AZOInterpreter *intr, unsigned int n_elements);
void azo_interpreter_pop_frame (AZOInterpreter *intr);
void azo_interpreter_clear_frame (AZOInterpreter *intr);
void azo_interpreter_restore_frame (AZOInterpreter *intr, unsigned int frame);

void interpreter_interpret (AZOInterpreter *intr, AZOProgram *prog, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size);

void azo_intepreter_print_stack (AZOInterpreter *intr, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
