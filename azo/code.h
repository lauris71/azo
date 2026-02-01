#ifndef __AZO_CODE_H__
#define __AZO_CODE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <az/packed-value.h>

#include <azo/expression.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZOCode AZOCode;

struct _AZOCode {
	/**
	 * @brief Bytecode buffer
	 * 
	 */
	unsigned int bc_len;
	unsigned int bc_size;
	uint8_t *bc;
    /**
     * @brief Expressions for generating debug data
     * 
     */
    const AZOExpression **exprs;
	/* Data */
	/* fixme: Implement as stack/array */
	unsigned int data_size;
	unsigned int data_len;
	AZPackedValue *data;
};

void azo_code_init(AZOCode *code, unsigned int debug);
void azo_code_clear(AZOCode *code);

/**
 * @brief Write bytes to bytecode block
 * 
 * @param code the code container
 * @param data pointert to the bytes
 * @param size the number of bytes
 * @param expr the current expression
 */
void azo_code_write_bc(AZOCode *code, const void *data, unsigned int size, const AZOExpression *expr);

static inline void
azo_code_write_ic(AZOCode *code, unsigned int ic, const AZOExpression *expr)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(code, &ic8, 1, expr);
}

static inline void
azo_code_write_ic_u8(AZOCode *code, unsigned int ic, unsigned int val, const AZOExpression *expr)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val};
	azo_code_write_bc(code, &ic8, 2, expr);
}

static inline void
azo_code_write_ic_u32 (AZOCode *code, uint8_t ic, uint32_t val, const AZOExpression *expr)
{
	azo_code_write_bc(code, &ic, 1, expr);
	azo_code_write_bc(code, &val, 4, expr);
}

static inline void
azo_code_write_ic_i32 (AZOCode *code, uint8_t ic, int32_t val, const AZOExpression *expr)
{
	azo_code_write_bc(code, &ic, 1, expr);
	azo_code_write_bc(code, &val, 4, expr);
}

static inline void
azo_code_write_ic_u8_u32 (AZOCode *code, unsigned int ic, unsigned int val1, unsigned int val2, const AZOExpression *expr)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val1};
	azo_code_write_bc(code, &ic8, 2, expr);
	azo_code_write_bc(code, &val2, 4, expr);
}

static inline void
azo_code_write_ic_u32_u32(AZOCode *code, unsigned int ic, unsigned int val1, unsigned int val2, const AZOExpression *expr)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(code, &ic8, 1, expr);
	azo_code_write_bc(code, &val1, 4, expr);
	azo_code_write_bc(code, &val2, 4, expr);
}

/**
 * @brief Write a value to data block
 * 
 * @param code the code container
 * @param impl the data implementation
 * @param inst the data instance
 * @return the index of the value in data block
 */
unsigned int azo_code_write_instance(AZOCode *code, const AZImplementation *impl, void *inst);

void azo_code_reserve_data (AZOCode *code, unsigned int amount);

int azo_code_find_block(AZOCode *code, const AZImplementation *impl, void *block);

#ifdef __cplusplus
}
#endif

#endif
