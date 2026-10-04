#ifndef __AZO_CODE_H__
#define __AZO_CODE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <string.h>

#include <az/packed-value.h>

#include <azo/node.h>
#include <azo/bytecode.h>

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
    const AZONode **exprs;
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
void azo_code_write_bc(AZOCode *code, const void *data, unsigned int size, const AZONode *expr);

static inline void
azo_code_write_ic(AZOCode *code, unsigned int ic, const AZONode *expr)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(code, &ic8, 1, expr);
}

static inline void
azo_code_write_ic_u8(AZOCode *code, unsigned int ic, unsigned int val, const AZONode *expr)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val};
	azo_code_write_bc(code, &ic8, 2, expr);
}

static inline void
azo_code_write_ic_u32 (AZOCode *code, unsigned int ic, uint32_t val, const AZONode *expr)
{
	azo_code_write_bc(code, &ic, 1, expr);
	azo_code_write_bc(code, &val, 4, expr);
}

static inline void
azo_code_write_ic_i32 (AZOCode *code, unsigned int ic, int32_t val, const AZONode *expr)
{
	azo_code_write_bc(code, &ic, 1, expr);
	azo_code_write_bc(code, &val, 4, expr);
}

static inline void
azo_code_write_ic_u8_u32 (AZOCode *code, unsigned int ic, unsigned int val1, uint32_t val2, const AZONode *expr)
{
	uint8_t ic8[] = {(uint8_t) ic, (uint8_t) val1};
	azo_code_write_bc(code, &ic8, 2, expr);
	azo_code_write_bc(code, &val2, 4, expr);
}

static inline void
azo_code_write_ic_u32_u32(AZOCode *code, unsigned int ic, uint32_t val1, uint32_t val2, const AZONode *expr)
{
	uint8_t ic8 = ic;
	azo_code_write_bc(code, &ic8, 1, expr);
	azo_code_write_bc(code, &val1, 4, expr);
	azo_code_write_bc(code, &val2, 4, expr);
}

static inline void
azo_code_write_EXCEPTION (AZOCode *code, uint32_t type, const AZONode *node)
{
	azo_code_write_ic_u32(code, AZO_TC_EXCEPTION, type, node);
}

static inline void
azo_code_write_POP (AZOCode *code, uint32_t n_values, const AZONode *expr)
{
	azo_code_write_ic_u32(code, AZO_TC_POP, n_values, expr);
}

static inline void
azo_code_write_REMOVE (AZOCode *code, unsigned int first, unsigned int n_values, const AZONode *expr)
{
	azo_code_write_ic_u32_u32(code, AZO_TC_REMOVE, first, n_values, expr);
}

static inline void
azo_code_write_PUSH_IMMEDIATE (AZOCode *code, unsigned int type, const AZValue *val, const AZONode *node)
{
	azo_code_write_ic_u8(code, AZO_TC_PUSH_IMMEDIATE, type, node);
	if (type) {
		unsigned int val_size = az_class_value_size(AZ_CLASS_FROM_TYPE(type));
		if (val_size) {
			azo_code_write_bc(code, val, val_size, node);
		}
	}
}

static inline void
azo_code_write_DUPLICATE (AZOCode *code, unsigned int pos, const AZONode *expr)
{
	azo_code_write_ic_u32(code, AZO_TC_DUPLICATE, pos, expr);
}

static inline void
azo_code_write_EXCHANGE (AZOCode *code, unsigned int pos, const AZONode *node)
{
	azo_code_write_ic_u32(code, AZO_TC_EXCHANGE, pos, node);
}

static inline void
azo_code_write_TEST_TYPE (AZOCode *code, unsigned int typecode, unsigned int pos, const AZONode *node)
{
	azo_code_write_ic_u8(code, typecode, pos, node);
}

static inline void
azo_code_write_TEST_TYPE_IMMEDIATE (AZOCode *code, unsigned int typecode, unsigned int pos, unsigned int type, const AZONode *node)
{
	azo_code_write_ic_u8_u32(code, typecode, pos, type, node);
}

static inline void
azo_code_write_TYPE_OF (AZOCode *code, unsigned int pos, const AZONode *node)
{
	azo_code_write_ic_u8(code, AZO_TC_TYPE_OF, pos, node);
}

static inline unsigned int
azo_code_write_JMP32 (AZOCode *code, unsigned int ic, unsigned int to, const AZONode *node)
{
	unsigned int pos = code->bc_len;
	int32_t raddr = (int) to - (int) (pos + 5);
	azo_code_write_ic_i32 (code, ic, raddr, node);
	return pos;
}

static inline void
azo_code_update_JMP32 (AZOCode *code, unsigned int loc)
{
	int32_t raddr = (int) code->bc_len - (int) (loc + 5);
	memcpy(code->bc + loc + 1, &raddr, 4);
}

static inline void
azo_code_write_PROMOTE (AZOCode *code, unsigned int pos, const AZONode *node)
{
	azo_code_write_ic_u8(code, AZO_TC_PROMOTE, pos, node);
}

static inline void
azo_code_write_EQUAL_TYPED (AZOCode *code, uint32_t type, const AZONode *node)
{
	azo_code_write_ic_u8(code, AZO_TC_EQUAL_TYPED, type, node);
}

static inline void
azo_code_write_COMPARE_TYPED (AZOCode *code, uint32_t type, const AZONode *node)
{
	azo_code_write_ic_u8(code, AZO_TC_COMPARE_TYPED, type, node);
}

static inline void
azo_code_write_MINMAX_TYPED (AZOCode *code, unsigned int typecode, uint32_t type, const AZONode *node)
{
	azo_code_write_ic_u8(code, typecode, type, node);
}

static inline void
azo_code_write_GET_ATTRIBUTE(AZOCode *code, const AZONode *node)
{
	azo_code_write_ic(code, AZO_TC_GET_ATTRIBUTE, node);
}

static inline void
azo_code_write_SET_ATTRIBUTE (AZOCode *code, const AZONode *node)
{
	azo_code_write_ic(code, AZO_TC_SET_ATTRIBUTE, node);
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
