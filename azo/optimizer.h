#ifndef __AZO_OPTIMIZER_H__
#define __AZO_OPTIMIZER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOCompiler AZOCompiler;

#include <azo/node.h>
#include <azo/source.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_COMPILER_NO_CONST_ASSIGN 1
#define AZO_COMPILER_VAR_IS_LVALUE 2

AZONode *azo_compiler_resolve_frame (AZOCompiler *comp, AZONode *root);

AZONode *azo_compiler_resolve_expression (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);

AZONode *azo_compiler_resolve_binary (AZONode *expr);
AZONode *azo_compiler_resolve_prefix (AZONode *expr);
AZONode *azo_compiler_resolve_array_literal (AZONode *expr);

AZONode *azo_compiler_resolve_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);
AZONode *azo_compiler_resolve_function_call (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);
AZONode *azo_compiler_resolve_new (AZOCompiler *comp, AZONode *expr, unsigned int flags, unsigned int *result);

#ifdef __cplusplus
}
#endif

#endif
