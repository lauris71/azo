#ifndef __AZO_OPTIMIZER_H__
#define __AZO_OPTIMIZER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;

#include <azo/expression.h>
#include <azo/source.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_COMPILER_NO_CONST_ASSIGN 1
#define AZO_COMPILER_VAR_IS_LVALUE 2

AZOExpression *azo_compiler_resolve_frame (AZOCompiler *comp, AZOExpression *expr);

AZOExpression *azo_compiler_resolve_expression (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result);

AZOExpression *azo_compiler_resolve_binary (AZOExpression *expr);
AZOExpression *azo_compiler_resolve_prefix (AZOExpression *expr);
AZOExpression *azo_compiler_resolve_array_literal (AZOExpression *expr);

AZOExpression *azo_compiler_resolve_reference (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result);
AZOExpression *azo_compiler_resolve_function_call (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result);
AZOExpression *azo_compiler_resolve_new (AZOCompiler *comp, AZOExpression *expr, unsigned int flags, unsigned int *result);

#ifdef __cplusplus
}
#endif

#endif
