#define __AZO_COMPILE_HELPERS_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/compiler/helpers.h>

void
azo_code_compile_type_is_in_range (AZOCode *code, unsigned int pos, uint32_t min_type, uint32_t max_type, const AZONode *node,  unsigned int *jmp_lt, unsigned int *jmp_gt)
{
	if (jmp_lt) {
		azo_code_write_TYPE_OF(code, pos, node);
		azo_code_write_PUSH_IMMEDIATE(code, AZ_TYPE_UINT32, (const AZValue *) &min_type, node);
		azo_code_write_COMPARE_TYPED(code, AZ_TYPE_UINT32, node);
		*jmp_lt = azo_code_write_JMP_32 (code, JMP_32_IF_NEGATIVE, 0, node);
	}
	if (jmp_gt) {
		azo_code_write_TYPE_OF(code, pos, node);
		azo_code_write_PUSH_IMMEDIATE(code, AZ_TYPE_UINT32, (const AZValue *) &max_type, node);
		azo_code_write_COMPARE_TYPED(code, AZ_TYPE_UINT32, node);
		*jmp_gt = azo_code_write_JMP_32 (code, JMP_32_IF_POSITIVE, 0, node);
	}
}

