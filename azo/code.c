#define __AZO_CODE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <assert.h>

#include <azo/code.h>

void
azo_code_init(AZOCode *code, unsigned int debug)
{
    memset(code, 0, sizeof(AZOCode));
    code->bc_size = 256;
    code->bc = (uint8_t *) malloc(code->bc_size);
    if (debug) {
        code->exprs = (const AZOExpression **) malloc(code->bc_size * sizeof(AZOExpression *));
    }
}

void
azo_code_clear(AZOCode *code)
{
	if (code->bc) free(code->bc);
    if (code->exprs) free(code->exprs);
	if (code->data) {
		unsigned int i;
		for (i = 0; i < code->data_len; i++) az_packed_value_clear(&code->data[i]);
		free (code->data);
	}
}

static void
azo_code_ensure_data(AZOCode *code, unsigned int amount)
{
	if ((code->data_len + amount) > code->data_size) {
		unsigned int new_size;
		new_size = code->data_size << 1;
		if (new_size < 16) new_size = 16;
		if (new_size < (code->data_len + amount)) new_size = code->data_len + amount;
		code->data = (AZPackedValue *) realloc(code->data, new_size * sizeof (AZPackedValue));
		memset (&code->data[code->data_size], 0, (new_size - code->data_size) * sizeof (AZPackedValue));
		code->data_size = new_size;
	}
}

void
azo_code_write_bc(AZOCode *code, const void *data, unsigned int size, const AZOExpression *expr)
{
    /* Ensure room at the end of bytecode buffer */
	if ((code->bc_len + size) > code->bc_size) {
		code->bc_size <<= 1;
		if (code->bc_size < 256) code->bc_size = 256;
		if (code->bc_size < (code->bc_len + size)) code->bc_size = code->bc_len + size;
		code->bc = (uint8_t *) realloc(code->bc, code->bc_size);
        if (code->exprs) code->exprs = (const AZOExpression **) realloc(code->exprs, code->bc_size * sizeof(AZOExpression *));
	}
	memcpy (code->bc + code->bc_len, data, size);
    if (code->exprs) {
        for (unsigned int i = 0; i < size; i++) {
            code->exprs[code->bc_len + i] = expr;
        }
    }
	code->bc_len += size;
}

unsigned int
azo_code_write_instance(AZOCode *code, const AZImplementation *impl, void *inst)
{
    azo_code_ensure_data(code, 1);
	az_packed_value_set(&code->data[code->data_len], impl, inst);
	return code->data_len++;
}

void
azo_code_reserve_data(AZOCode *code, unsigned int amount)
{
	assert(code->data_len == 0);
    azo_code_ensure_data(code, amount);
	code->data_len = amount;
}

int
azo_code_find_block(AZOCode *code, const AZImplementation *impl, void *block)
{
    for (unsigned int i = 0; i < code->data_len; i++) {
        if (code->data[i].impl != impl) continue;
        if (code->data[i].v.block != block) continue;
        return i;
    }
    return -1;
}
