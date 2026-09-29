#ifndef __AZO_RESOLVER_H__
#define __AZO_RESOLVER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

typedef struct _AZOCompilerContext AZOResolveCtx;

#include <azo/node.h>
#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int azo_compiler_resolve_node(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *expr);

int azo_compiler_resolve_reference(AZOCompiler *comp, AZOResolveCtx *rct, AZONode *expr);
/**
 * @brief Resolve type expression to TYPE term
 * 
 * @param comp The compiler
 * @param node The current node
 * @param flags Resolver flags
 * @return int 0 on success, non-zero on error
 */
int azo_compiler_resolve_type_expression(AZOCompiler *comp, AZOResolveCtx *rctx, AZONode *node);

#ifdef __cplusplus
}
#endif

#endif
