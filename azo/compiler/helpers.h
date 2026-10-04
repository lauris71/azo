#ifndef __AZO_COMPILE_HELPERS_H__
#define __AZO_COMPILE_HELPERS_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#include <azo/code.h>

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


#ifdef __cplusplus
}
#endif

#endif
