#ifndef __AZO_INTERPRETER_H__
#define __AZO_INTERPRETER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

typedef struct _AZOInterpreter AZOInterpreter;
typedef struct _AZOInterpreterCtx AZOInterpreterCtx;

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

#define AZO_INTR_FLAG_CHECK_ARGS 1

struct _AZOInterpreter {
	AZOContext *ctx;

	/* Current stack frame start */
	unsigned int n_frames;
	unsigned int size_frames;
	unsigned int *frames;
	AZOStack stack;
	uint32_t flags;
	AZOException exc;
	/* Scratch area */
	/* vals[0] is used to store return value */
	AZPackedValue64 vals[2];
};

struct _AZOInterpreterCtx {
	const uint8_t *tcode;
	unsigned int tcode_len;
	AZODataBlock *static_data;
	AZODataBlock *shared_data;
	AZODebugInfo *debug;
};

AZOInterpreter *azo_interpreter_new (AZOContext *ctx);
void interpreter_delete (AZOInterpreter *intr);

void azo_intepreter_push_instance (AZOInterpreter *intr, const AZImplementation *impl, void *inst);
void azo_intepreter_push_value (AZOInterpreter *intr, const AZImplementation *impl, const AZValue *val);
void azo_intepreter_push_values (AZOInterpreter *intr, const AZImplementation **impls, const AZValue **vals, unsigned int n_vals);

/**
 * @brief Pushes a new frame to the top of the frame stack
 * 
 * The new frame pointer is set to contain n_elements values from the top of the stack
 * 
 * @param intr the interpreter
 * @param n_elements the number of elements to reserve
 * @return the previous frame pointer
 */
unsigned int azo_interpreter_push_frame (AZOInterpreter *intr, unsigned int n_elements);
void azo_interpreter_pop_frame (AZOInterpreter *intr);
void azo_interpreter_clear_frame (AZOInterpreter *intr);
void azo_interpreter_restore_frame (AZOInterpreter *intr, unsigned int frame);

void azo_interpreter_exception(AZOInterpreter *intr, const uint8_t *ip, unsigned int type);

const uint8_t *azo_interpreter_interpret_tc (AZOInterpreter *intr, AZOInterpreterCtx *ictx, const uint8_t *ipc);

void azo_interpreter_run(AZOInterpreter *intr, AZOInterpreterCtx *ictx);

void azo_intepreter_print_stack (AZOInterpreter *intr, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
