#ifndef __AZO_COMPILER_H__
#define __AZO_COMPILER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;

#include <stdint.h>

#include <azo/context.h>
#include <azo/compiler/frame.h>
#include <azo/expression.h>
#include <azo/interpreter.h>
#include <azo/source.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOCompiler {
	/**
	 * @brief Global definitions
	 * 
	 */
	AZOContext *ctx;
	/**
	 * @brief Force typecode argument checking
	 * 
	 * This is only needed to debug compiler/interpreter
	 * 
	 */
	unsigned int check_args : 1;
	/**
	 * @brief Write debug symbols
	 * 
	 */
	unsigned int debug : 1;
	/**
	 * @brief Current compilation frame
	 * 
	 */
	AZOFrame *current;
};

void azo_compiler_init (AZOCompiler *compiler, AZOContext *ctx);
void azo_compiler_finalize (AZOCompiler *compiler);

AZOProgram *azo_compiler_compile (AZOCompiler *comp, AZOExpression *root, const AZOSource *src);

void azo_compiler_push_frame (AZOCompiler *comp, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type);
AZOFrame *azo_compiler_pop_frame (AZOCompiler *comp);

/* Declares variable at next free position unless already known */
void azo_compiler_declare_variable (AZOCompiler *comp, AZString *name, unsigned int type);

void azo_compiler_write_instruction_1 (AZOCompiler *comp, unsigned int ic);

void azo_compiler_write_DEBUG_STACK (AZOCompiler *comp);
void azo_compiler_write_DEBUG_STRING (AZOCompiler *comp, const char *text);

void azo_compiler_write_EXCEPTION (AZOCompiler *comp, uint32_t type);
void azo_compiler_write_EXCEPTION_C (AZOCompiler *comp, unsigned int tc, uint32_t type);
void azo_compiler_write_POP (AZOCompiler *comp, uint32_t n_values);
void azo_compiler_write_REMOVE (AZOCompiler *comp, unsigned int first, unsigned int n_values);
void azo_compiler_write_PUSH_IMMEDIATE (AZOCompiler *comp, unsigned int type, const AZValue *value);
void azo_compiler_write_PUSH_EMPTY (AZOCompiler *comp, uint32_t type);
void azo_compiler_write_DUPLICATE (AZOCompiler *comp, unsigned int pos);
void azo_compiler_write_DUPLICATE_FRAME (AZOCompiler *comp, unsigned int pos);
void azo_compiler_write_EXCHANGE (AZOCompiler *comp, unsigned int pos);
void azo_compiler_write_TEST_TYPE (AZOCompiler *comp, unsigned int typecode, unsigned int pos);
void azo_compiler_write_TEST_TYPE_IMMEDIATE (AZOCompiler *comp, unsigned int typecode, unsigned int pos, unsigned int type);
void azo_compiler_write_TYPE_OF (AZOCompiler *comp, unsigned int pos);
unsigned int azo_compiler_write_JMP_32 (AZOCompiler *comp, unsigned int typecode, unsigned int to);
void azo_compiler_update_JMP_32 (AZOCompiler *comp, unsigned int from);
void azo_compiler_write_PROMOTE (AZOCompiler *comp, uint8_t pos);
void azo_compiler_write_EQUAL_TYPED (AZOCompiler *comp, uint8_t type);
void azo_compiler_write_COMPARE_TYPED (AZOCompiler *comp, uint8_t type);
void azo_compiler_write_ARITHMETIC_TYPED (AZOCompiler *comp, unsigned int typecode, uint8_t type);
void azo_compiler_write_MINMAX_TYPED (AZOCompiler *comp, unsigned int typecode, uint8_t type);

unsigned int azo_compiler_compile_expression (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);

#ifdef __cplusplus
}
#endif

#endif
