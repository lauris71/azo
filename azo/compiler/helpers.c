#define __AZO_COMPILE_HELPERS_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/compiler/helpers.h>

void
azo_code_compile_type_is_in_range (AZOCompilerContext *ctx, AZOCode *code, unsigned int pos, uint32_t min_type, uint32_t max_type, const AZONode *node,  unsigned int *jmp_lt, unsigned int *jmp_gt)
{
	if (jmp_lt) {
		azo_code_write_TYPE_OF(code, pos, node);
		ctx->n_stack += 1;
		azo_code_write_PUSH_IMMEDIATE(code, AZ_TYPE_UINT32, (const AZValue *) &min_type, node);
		ctx->n_stack += 1;
		azo_code_write_COMPARE_TYPED(code, AZ_TYPE_UINT32, node);
		ctx->n_stack -= 1;
		*jmp_lt = azo_code_write_JMP32 (code, JMP_32_IF_NEGATIVE, 0, node);
		ctx->n_stack -= 1;
	}
	if (jmp_gt) {
		azo_code_write_TYPE_OF(code, pos, node);
		ctx->n_stack += 1;
		azo_code_write_PUSH_IMMEDIATE(code, AZ_TYPE_UINT32, (const AZValue *) &max_type, node);
		ctx->n_stack += 1;
		azo_code_write_COMPARE_TYPED(code, AZ_TYPE_UINT32, node);
		ctx->n_stack -= 1;
		*jmp_gt = azo_code_write_JMP32 (code, JMP_32_IF_POSITIVE, 0, node);
		ctx->n_stack -= 1;
	}
}

void
azo_code_compile_type_is_immediate (AZOCompilerContext *ctx, AZOCode *code, unsigned int tc, unsigned int pos, unsigned int type, unsigned int *jmp_if, unsigned int *jmp_if_not, const AZONode *expr)
{
	azo_code_write_TEST_TYPE_IMMEDIATE (code, tc, pos, type, expr);
	ctx->n_stack += 1;
	if (jmp_if) {
		*jmp_if = azo_code_write_JMP32 (code, JMP_32_IF, 0, expr);
		ctx->n_stack -= 1;
		if (jmp_if_not) {
			*jmp_if_not = azo_code_write_JMP32 (code, JMP_32, 0, expr);
		}
	} else if (jmp_if_not) {
		*jmp_if_not = azo_code_write_JMP32 (code, JMP_32_IF_NOT, 0, expr);
		ctx->n_stack -= 1;
	}
}

