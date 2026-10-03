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

#ifdef __cplusplus
}
#endif

#endif
