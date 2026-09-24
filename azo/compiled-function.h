#ifndef __AZO_COMPILED_FUNCTION_H__
#define __AZO_COMPILED_FUNCTION_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define AZO_TYPE_COMPILED_FUNCTION azo_compiled_function_get_type ()
#define AZO_COMPILED_FUNCTION(o) AZ_CHECK_INSTANCE_CAST ((o), AZO_TYPE_COMPILED_FUNCTION, AZOCompiledFunction))
#define AZO_IS_COMPILED_FUNCTION(o) AZ_CHECK_INSTANCE_TYPE ((o), AZO_TYPE_COMPILED_FUNCTION))

#define AZO_COMPILED_FUNCTION_FROM_FUNCTION_INSTANCE(i) (AZOCompiledFunction *) ARIKKEI_BASE_ADDRESS (AZOCompiledFunction, function_inst, i)

typedef struct _AZOCompiledFunction AZOCompiledFunction;
typedef struct _AZOCompiledFunctionClass AZOCompiledFunctionClass;

#include <az/function.h>
#include <az/object.h>

#include <azo/node.h>
#include <azo/interpreter.h>
#include <azo/datablock.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOCompiledFunction {
	AZObject object;
	AZFunctionSignature *signature;
	AZOContext *ctx;
	AZONode *root;
	/* Captures and static */
	unsigned int n_captures;
	unsigned int n_static;
	AZODataBlock static_data;
	/* Code */
	AZOProgram *prog;
};

struct _AZOCompiledFunctionClass {
	AZObjectClass object_class;
	/* ArikkeiFunction implementation */
	AZFunctionImplementation function_impl;
};

unsigned int azo_compiled_function_get_type (void);

AZOCompiledFunction *azo_compiled_function_new(AZOProgram *program);

/**
 * @brief Write an actual value to the reserved parent variable slot
 * 
 * @param cfunc A compiled function
 * @param pos The parent variable slot
 * @param impl The value implementation
 * @param inst The value instance
 */
void azo_compiled_function_bind (AZOCompiledFunction *cfunc, unsigned int pos, const AZImplementation *impl, void *inst);

#ifdef __cplusplus
};
#endif

#endif

