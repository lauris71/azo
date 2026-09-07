#ifndef __AZO_COMPILER_H__
#define __AZO_COMPILER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;
typedef struct _AZOCompilerContext AZOCompilerContext;

#include <stdint.h>

#include <azo/context.h>
#include <azo/compiler/frame.h>
#include <azo/node.h>
#include <azo/interpreter.h>
#include <azo/source.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOCompilerContext {
	/**
	 * @brief Global definitions
	 * 
	 */
	AZOContext *globals;
	/**
	 * @brief Local variables
	 * 
	 */
	const AZImplementation *this_impl;
	void *this_inst;
	unsigned int ret_type;
	unsigned int n_args;
	AZString **arg_names;
	const unsigned int *arg_types;
};

struct _AZOCompiler {
	/** Context
	 * 
	 */
	AZOCompilerContext *ctx;
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

void azo_compiler_init (AZOCompiler *compiler, AZOCompilerContext *ctx);
void azo_compiler_finalize (AZOCompiler *compiler);

AZOProgram *azo_compiler_compile (AZOCompiler *comp, AZONode *root, unsigned int need_resolve, AZOSource *src);

/**
 * @brief Start new current frame, preserving link to parent
 *
 * I.e. start compiling an outermost program body or resolve function definition inside code
 * 
 * @param comp The compiler.
 * @param this_impl The implementation of this (or NULL for none).
 * @param this_inst The instance of this (or NULL if none/not defined).
 * @param ret_type The return type of the code.
 */
void azo_compiler_push_frame (AZOCompiler *comp, const AZImplementation *this_impl, void *this_inst, unsigned int ret_type);
/**
 * @brief Set the new current frame, removing all references to parent
 * 
 * I.e. start compiling the resolved function definition using it's resolved frame
 * 
 * @param comp The compiler.
 * @param frame The frame to set as current.
 * @return AZOFrame* The previous current frame.
 */
AZOFrame *azo_compiler_set_frame (AZOCompiler *comp, AZOFrame *frame);
AZOFrame *azo_compiler_pop_frame (AZOCompiler *comp);

/* Declares variable at next free position unless already known */
void azo_compiler_declare_variable (AZOCompiler *comp, AZString *name, unsigned int type);

void azo_compiler_write_ic (AZOCompiler *comp, unsigned int ic, const AZONode *expr);

void azo_compiler_write_DEBUG_STACK (AZOCompiler *comp);
void azo_compiler_write_DEBUG_STRING (AZOCompiler *comp, const char *text, const AZONode *expr);

void azo_compiler_write_EXCEPTION (AZOCompiler *comp, uint32_t type, const AZONode *expr);
void azo_compiler_write_EXCEPTION_C (AZOCompiler *comp, unsigned int tc, uint32_t type);
void azo_compiler_write_POP (AZOCompiler *comp, uint32_t n_values, const AZONode *expr);
void azo_compiler_write_REMOVE (AZOCompiler *comp, unsigned int first, unsigned int n_values, const AZONode *expr);
void azo_compiler_write_PUSH_IMMEDIATE (AZOCompiler *comp, unsigned int type, const AZValue *value, const AZONode *expr);
void azo_compiler_write_PUSH_EMPTY (AZOCompiler *comp, uint32_t type, const AZONode *expr);
void azo_compiler_write_DUPLICATE (AZOCompiler *comp, unsigned int pos, const AZONode *expr);
void azo_compiler_write_EXCHANGE (AZOCompiler *comp, unsigned int pos);
void azo_compiler_write_TEST_TYPE (AZOCompiler *comp, unsigned int typecode, unsigned int pos);
void azo_compiler_write_TEST_TYPE_IMMEDIATE (AZOCompiler *comp, unsigned int typecode, unsigned int pos, unsigned int type, const AZONode *expr);
void azo_compiler_write_TYPE_OF (AZOCompiler *comp, unsigned int pos);
unsigned int azo_compiler_write_JMP_32 (AZOCompiler *comp, unsigned int typecode, unsigned int to, const AZONode *expr);
void azo_compiler_update_JMP_32 (AZOCompiler *comp, unsigned int from);
void azo_compiler_write_PROMOTE (AZOCompiler *comp, uint8_t pos);
void azo_compiler_write_EQUAL_TYPED (AZOCompiler *comp, uint32_t type);
void azo_compiler_write_COMPARE_TYPED (AZOCompiler *comp, uint32_t type);
void azo_compiler_write_ARITHMETIC_TYPED (AZOCompiler *comp, unsigned int typecode, uint32_t type);
void azo_compiler_write_MINMAX_TYPED (AZOCompiler *comp, unsigned int typecode, uint32_t type);

unsigned int azo_compiler_compile_expression (AZOCompiler *comp, const AZONode *expr, AZOSource *src);

#ifdef __cplusplus
}
#endif

#endif
