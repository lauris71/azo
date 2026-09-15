#ifndef __AZO_OPTIMIZER_H__
#define __AZO_OPTIMIZER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;
typedef struct _AZOOptimizer AZOOptimizer;

#include <azo/node.h>
#include <azo/source.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_OPTIMIZER_FLAG_CALC_CONST_EXPRESSIONS 1

#define AZO_OPTIMIZER_FLAG_ALL AZO_OPTIMIZER_FLAG_CALC_CONST_EXPRESSIONS
struct _AZOOptimizer {
	AZOCompiler *comp;
};

int azo_compiler_optimize (AZOOptimizer *opt, AZONode *node, unsigned int flags);

int azo_compiler_optimize_constant_binary (AZOOptimizer *opt, AZONode *expr);

#define AZO_COMPILER_NO_CONST_ASSIGN 1
#define AZO_COMPILER_VAR_IS_LVALUE 2

AZONode *azo_compiler_resolve_node (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);

AZONode *azo_compiler_resolve_prefix (AZONode *expr);
AZONode *azo_compiler_resolve_array_literal (AZONode *expr);

int azo_compiler_resolve_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags);
unsigned int azo_compiler_resolve_node_to_class(AZOCompiler *comp, AZONode *expr, unsigned int flags);
AZONode *azo_compiler_resolve_function_call (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);
AZONode *azo_compiler_resolve_new (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);

#ifdef __cplusplus
}
#endif

#endif
