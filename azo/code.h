#ifndef __AZO_CODE_H__
#define __AZO_CODE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <az/packed-value.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOCode AZOCode;

struct _AZOCode {
	/* Bytecode */
	unsigned int bc_len;
	unsigned int bc_size;
	uint8_t *bc;
	/* Data */
	/* fixme: Implement as stack/array */
	unsigned int data_size;
	unsigned int data_len;
	AZPackedValue *data;
};

void azo_code_clear(AZOCode *code);

void azo_code_write_bc(AZOCode *code, void *data, unsigned int size);
unsigned int azo_code_write_instance(AZOCode *code, const AZImplementation *impl, void *inst);

void azo_code_reserve_data (AZOCode *code, unsigned int amount);

int azo_code_find_block(AZOCode *code, const AZImplementation *impl, void *block);

#ifdef __cplusplus
}
#endif

#endif
