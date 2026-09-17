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
    unsigned int n_const_subst;
};

void azo_optimizer_setup(AZOOptimizer *opt, AZOCompiler *comp);
void azo_optimizer_release(AZOOptimizer *opt);

int azo_compiler_optimize(AZOOptimizer *opt, AZONode *root, unsigned int flags);

int azo_compiler_calculate_constant_binary(AZOOptimizer *opt, AZONode *expr);
int azo_compiler_calculate_rvalue_prefix(AZOOptimizer *opt, AZONode *node);

#ifdef __cplusplus
}
#endif

#endif
