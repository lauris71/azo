#ifndef __AZO_COMPILE_HELPERS_H__
#define __AZO_COMPILE_HELPERS_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/code.h>
#include <azo/compiler/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

void azo_code_compile_type_is_in_range (AZOCode *code, unsigned int pos, uint32_t min_type, uint32_t max_type, const AZONode *node, unsigned int *jmp_lt, unsigned int *jmp_gt);
void azo_code_compile_type_is_immediate (AZOCode *code, unsigned int tc, unsigned int pos, unsigned int type, unsigned int *jmp_if, unsigned int *jmp_if_not, const AZONode *expr);

static inline void
azo_code_compile_last_is_none (AZOCode *code, unsigned int *jmp_if, unsigned int *jmp_if_not, const AZONode *expr)
{
	azo_code_compile_type_is_immediate (code, AZO_TC_TYPE_EQUALS_IMMEDIATE, 0, AZ_TYPE_NONE, jmp_if, jmp_if_not, expr);
}

static unsigned int
azo_code_compile_expression_and_type_check(AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *expr, unsigned int pos, unsigned int type, AZOSource *src)
{
	if (!azo_compiler_compile_expression (comp, ctx, expr, src)) return 0;
	azo_code_write_ic_u32_u32 (&ctx->frame->code, AZO_TC_EXCEPTION_IF_TYPE_IS_NOT, pos, type, expr);
	return 1;
}

#ifdef AZO_TC_HAS_DEBUG
static inline void
azo_compiler_write_DEBUG_STACK (AZOCompiler *comp, AZOCompilerContext *ctx, const AZONode *node)
{
	azo_code_write_ic_u32 (&ctx->frame->code, AZO_TC_DEBUG_STACK, 0, node);
}

static inline void
azo_compiler_write_DEBUG_STRING (AZOCompiler *comp, AZOCompilerContext *ctx, const char *text, const AZONode *node)
{
	AZString *str = az_string_new((const uint8_t *) text);
	unsigned int pos = azo_frame_append_string(ctx->frame, str);
	azo_code_write_ic_u32 (&ctx->frame->code, AZO_TC_DEBUG_STR, pos, node);
	az_string_unref (str);
}

#else
#define azo_code_write_DEBUG_STACK(comp, ctx, node)
#define azo_code_write_DEBUG_STRING(comp, ctx, text, node)
#endif

#ifdef __cplusplus
}
#endif

#endif
