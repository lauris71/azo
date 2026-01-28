#ifndef __AZO_COMPARE_H__
#define __AZO_COMPARE_H__

/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2016-2018
 */

#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

	unsigned int azo_compiler_compile_comparison (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src, unsigned int reg);

#ifdef __cplusplus
}
#endif

#endif
