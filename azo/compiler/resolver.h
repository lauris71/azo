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
/**
 * @brief Resolve type expression to TYPE term
 * 
 * @param comp The compiler
 * @param node The current node
 * @param flags Resolver flags
 * @return int 0 on success, non-zero on error
 */
int azo_compiler_resolve_type_expression(AZOCompiler *comp, AZONode *node, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif
