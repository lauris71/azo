#ifndef __AZO_ARITHMETIC_H__
#define __AZO_ARITHMETIC_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int azo_compiler_compile_arithmetic (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *rhs, const AZOExpression *expr, const AZOSource *src);

/* Either bitwise not or complex conjugate depending on type */
unsigned int azo_compiler_compile_tilde (AZOCompiler *comp, const AZOExpression *expr, const AZOSource *src);

unsigned int azo_compiler_compile_increment (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *expr, const AZOSource *src);
unsigned int azo_compiler_compile_decrement (AZOCompiler *comp, const AZOExpression *lhs, const AZOExpression *expr, const AZOSource *src);

#ifdef __cplusplus
}
#endif

#endif
