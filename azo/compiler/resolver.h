#ifndef __AZO_RESOLVER_H__
#define __AZO_RESOLVER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <azo/node.h>
#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_COMPILER_VAR_IS_LVALUE 1

unsigned int azo_compiler_resolve_node (AZOCompiler *comp, AZONode *expr, unsigned int flags);

int azo_compiler_resolve_reference (AZOCompiler *comp, AZONode *expr, unsigned int flags);
unsigned int azo_compiler_resolve_node_to_class(AZOCompiler *comp, AZONode *expr, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif
