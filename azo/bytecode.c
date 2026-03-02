#define __AZO_BYTECODE_C__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <arikkei/arikkei-strlib.h>

#include <az/types.h>

#include "interpreter.h"

#include <azo/bytecode.h>

typedef struct _AZOBCInfo AZOBCInfo;

enum BCArgs {
	ARG_NONE,
	ARG_U8,
	ARG_U8_U32,
	ARG_U32,
	ARG_U32_U32,
	ARG_ADDR32,
	ARG_TYPE32,
	ARG_VALUE32,
	ARG_TYPE8_VALUE
};

struct _AZOBCInfo {
	unsigned int bc;
	const char *code;
	unsigned int args;
};

AZOBCInfo bc_info[] = {
	{NOP, "NOP", ARG_NONE},
	{AZO_TC_EXCEPTION, "EXCEPTION", ARG_U32},
	{AZO_TC_EXCEPTION_IF, "EXCEPTION IF", ARG_U32},
	{AZO_TC_EXCEPTION_IF_NOT, "EXCEPTION IFNOT", ARG_U32},
	{AZO_TC_EXCEPTION_IF_TYPE_IS_NOT, "EXCEPTION TYPEISNOT", ARG_U8_U32},
	{AZO_TC_DEBUG, "DEBUG", ARG_U32},
	{AZO_TC_DEBUG_STR, "DEBUG STR", ARG_U32},
	{AZO_TC_PUSH_FRAME, "PUSH FRAME", ARG_U32},
	{AZO_TC_POP_FRAME, "POP FRAME", ARG_NONE},
	{AZO_TC_POP, "POP", ARG_U32},
	{AZO_TC_REMOVE, "REMOVE", ARG_U32_U32},
	{AZO_TC_PUSH_EMPTY, "PUSH EMPTY", ARG_TYPE32},
	{PUSH_IMMEDIATE, "PUSH IMMEDIATE", ARG_TYPE8_VALUE},
	{AZO_TC_PUSH_VALUE, "PUSH VALUE", ARG_VALUE32},
	{AZO_TC_DUPLICATE, "DUPLICATE", ARG_U32},
	{AZO_TC_DUPLICATE_FRAME, "DUPLICATE FRAME", ARG_U32},
	{AZO_TC_EXCHANGE, "EXCHANGE", ARG_U32},
	{AZO_TC_EXCHANGE_FRAME, "EXCHANGE FRAME", ARG_U32},

	{AZO_TC_TYPE_EQUALS, "TYPE EQUALS", ARG_U8},
	{AZO_TC_TYPE_IS, "TYPE IS", ARG_U8},
	{AZO_TC_TYPE_IS_SUPER, "TYPE IS SUPER", ARG_U8},
	{AZO_TC_TYPE_IMPLEMENTS, "TYPE IMPLEMENTS", ARG_U8},
	{AZO_TC_TYPE_EQUALS_IMMEDIATE, "TYPE EQUALS IMMEDIATE", ARG_U8_U32},
	{AZO_TC_TYPE_IS_IMMEDIATE, "TYPE IS IMMEDIATE", ARG_U8_U32},
	{AZO_TC_TYPE_IS_SUPER_IMMEDIATE, "TYPE IS SUPER IMMEDIATE", ARG_U8_U32},
	{AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE, "TYPE IMPLEMENTS IMMEDIATE", ARG_U8_U32},
	{TYPE_OF, "TYPE OF", ARG_U8},
	{AZO_TC_TYPE_OF_CLASS, "TYPE OF CLASS", ARG_U8},

	{JMP_32, "JMP", ARG_ADDR32},
	{JMP_32_IF, "JMP IF", ARG_ADDR32},
	{JMP_32_IF_NOT, "JMP IFNOT", ARG_ADDR32},
	{JMP_32_IF_ZERO, "JMP IF ZERO", ARG_ADDR32},
	{JMP_32_IF_POSITIVE, "JMP IF POSITIVE", ARG_ADDR32},
	{JMP_32_IF_NEGATIVE, "JMP IF NEGATIVE", ARG_ADDR32},

	{PROMOTE, "PROMOTE", ARG_U8},
	{EQUAL_TYPED, "EQUAL TYPED", ARG_U8},
	{EQUAL, "EQUAL", ARG_NONE},
	{COMPARE_TYPED, "COMPARE_TYPED", ARG_U8},
	{COMPARE, "COMPARE", ARG_NONE},

	{AZO_TC_LOGICAL_NOT, "NOT", ARG_NONE},
	{AZO_TC_NEGATE, "NEGATE", ARG_NONE},
	{AZO_TC_CONJUGATE, "CONJUGATE", ARG_NONE},
	{AZO_TC_BITWISE_NOT, "BITWISE NOT", ARG_NONE},

	{AZO_TC_LOGICAL_AND, "AND", ARG_NONE},
	{AZO_TC_LOGICAL_OR, "OR", ARG_NONE},

	{AZO_TC_ADD_TYPED, "ADD TYPED", ARG_U8},
	{AZO_TC_SUBTRACT_TYPED, "SUB TYPED", ARG_U8},
	{AZO_TC_MULTIPLY_TYPED, "MUL TYPED", ARG_U8},
	{AZO_TC_DIVIDE_TYPED, "DIV TYPED", ARG_U8},
	{AZO_TC_MODULO_TYPED, "MOD TYPED", ARG_U8},
	{AZO_TC_ADD, "ADD", ARG_NONE},
	{AZO_TC_SUBTRACT, "SUB", ARG_NONE},
	{AZO_TC_MULTIPLY, "MUL", ARG_NONE},
	{AZO_TC_DIVIDE, "DIV", ARG_NONE},
	{AZO_TC_MODULO, "MOD", ARG_NONE},
	{MIN_TYPED, "MIN TYPED", ARG_U8},
	{MAX_TYPED, "MAX TYPED", ARG_U8},

	{AZO_TC_GET_INTERFACE_IMMEDIATE, "GET INTERFACE IMMEDIATE", ARG_U32},

	{AZO_TC_INVOKE, "INVOKE", ARG_U8},
	{AZO_TC_RETURN, "RETURN", ARG_NONE},
	{AZO_TC_RETURN_VALUE, "RETURN VALUE", ARG_NONE},
	{AZO_TC_BIND, "BIND", ARG_U32},

	{NEW_ARRAY, "NEW ARRAY", ARG_NONE},
	{LOAD_ARRAY_ELEMENT, "LOAD ARRAY ELEMENT", ARG_NONE},
	{WRITE_ARRAY_ELEMENT, "WRITE ARRAY ELEMENT"},

	{AZO_TC_GET_GLOBAL, "GET GLOBAL", ARG_NONE},
	{AZO_TC_GET_PROPERTY, "GET PROPERTY", ARG_NONE},
	{AZO_TC_GET_FUNCTION, "GET FUNCTION", ARG_U8},
	{AZO_TC_SET_PROPERTY, "SET PROPERTY", ARG_NONE},
	{AZO_TC_GET_STATIC_PROPERTY, "GET STATIC PROPERTY", ARG_NONE},
	{AZO_TC_GET_STATIC_FUNCTION, "GET STATIC FUNCTION", ARG_U8},
	{AZO_TC_LOOKUP_PROPERTY, "LOOKUP_PROPERTY", ARG_NONE},
	{GET_ATTRIBUTE, "GET ATTRIBUTE", ARG_NONE},
	{AZO_TC_SET_ATTRIBUTE, "SEt ATTRIBUTE", ARG_NONE}
};

static AZOBCInfo
get_bc_info(unsigned int bc)
{
	static unsigned int initialized = 0;
	if (!initialized) {
		for (unsigned int i = 0; i < AZO_TC_END; i++) {
			if (bc_info[i].bc != i) {
				fprintf(stderr, "Invalid typecode info: %d != %d\n", bc_info[i].bc, i);
			}
		}
		initialized = 1;
	}
	return bc_info[bc & 127];
}

static const uint8_t *
az_type_get_name(unsigned int type)
{
	AZClass *klass = az_type_get_class(type);
	return klass->name;
}

#define CHECK_PRINT_BC_LEN(d,d_len,len,v) if (v > len) return arikkei_strncpy(d, d_len, (const uint8_t *) "Unexpected end of instruction stream");

unsigned int
azo_bc_print_instruction(uint8_t *d, unsigned int d_len, const uint8_t *bc, unsigned int pos, unsigned int len)
{
	CHECK_PRINT_BC_LEN(d, d_len, len, 1);
	const uint8_t *ipc = bc + pos;
	const AZOBCInfo bci = get_bc_info(ipc[0]);
	const char *mod = (ipc[0] & 128) ? "*" : " ";
	unsigned int p = arikkei_strncpy(d, d_len, (const uint8_t *) mod);
	p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) bci.code);
	uint8_t b0[256], b1[256];
	uint32_t u32a, u32b;
	const AZClass *klass;
	switch(bci.args) {
		case ARG_NONE:
			break;
		case ARG_U8:
			CHECK_PRINT_BC_LEN(d, d_len, len, 2);
			arikkei_itoa(b0, 256, ipc[1]);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			break;
		case ARG_U8_U32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 6);
			arikkei_itoa(b0, 256, ipc[1]);
			memcpy(&u32a, ipc + 2, 4);
			arikkei_itoa(b1, 256, u32a);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b1);
			break;
		case ARG_U32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 5);
			memcpy(&u32a, ipc + 1, 4);
			arikkei_itoa(b0, 256, u32a);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			break;
		case ARG_U32_U32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 9);
			memcpy(&u32a, ipc + 1, 4);
			arikkei_itoa(b0, 256, u32a);
			memcpy(&u32b, ipc + 5, 4);
			arikkei_itoa(b1, 256, u32b);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b1);
			break;
		case ARG_ADDR32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 5);
			memcpy(&u32a, ipc + 1, 4);
			arikkei_itoa(b0, 256, pos + 5 + u32a);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			break;
		case ARG_TYPE32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 5);
			memcpy(&u32a, ipc + 1, 4);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, az_type_get_name(u32a));
			break;
		case ARG_VALUE32:
			CHECK_PRINT_BC_LEN(d, d_len, len, 5);
			memcpy(&u32a, ipc + 1, 4);
			arikkei_itoa(b0, 256, u32a);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			p += arikkei_strncpy(d + p, d_len - p, b0);
			break;
		case ARG_TYPE8_VALUE:
			CHECK_PRINT_BC_LEN(d, d_len, len, 2);
			u32a = ipc[1];
			klass = (u32a) ? AZ_CLASS_FROM_TYPE(u32a) : NULL;
			if (klass) CHECK_PRINT_BC_LEN(d, d_len, len, 2 + az_class_value_size(klass));
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
			if (klass) {
				p += arikkei_strncpy(d + p, d_len - p, az_type_get_name(u32a));
				p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) " ");
				AZValue val;
				memcpy(&val, ipc + 2, az_class_value_size(klass));
				p += az_instance_to_string(&klass->impl, &val, d + p, d_len - p);
			} else {
				p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) "NONE");
			}
			break;
		default:
			fprintf(stderr, "Invalid argument signature: %d\n", bci.args);
			p += arikkei_strncpy(d + p, d_len - p, (const uint8_t *) "INVALID ARGUMENT SIGNATURE");
			break;
	}
	return p;
}

unsigned int
azo_bc_next_instruction(const uint8_t *bc, unsigned int pos, unsigned int len)
{
	if ((pos + 1) >= len) return len;
	const uint8_t *ipc = bc + pos;
	const AZOBCInfo bci = get_bc_info(ipc[0]);
	switch(bci.args) {
		uint32_t u32a, u32b;
		const AZClass *klass;
		case ARG_NONE:
			pos += 1;
			break;
		case ARG_U8:
			if ((pos + 2) >= len) return len;
			pos += 2;
			break;
		case ARG_U8_U32:
			if ((pos + 6) >= len) return len;
			pos += 6;
			break;
		case ARG_U32:
			if ((pos + 5) >= len) return len;
			pos += 5;
			break;
		case ARG_U32_U32:
			if ((pos + 9) >= len) return len;
			pos += 9;
			break;
		case ARG_ADDR32:
		case ARG_TYPE32:
		case ARG_VALUE32:
			if ((pos + 5) >= len) return len;
			pos += 5;
			break;
		case ARG_TYPE8_VALUE:
			if ((pos + 2) >= len) return len;
			u32a = ipc[1];
			klass = (u32a) ? AZ_CLASS_FROM_TYPE(u32a) : NULL;
			pos += 2;
			if (klass) {
				if ((pos + az_class_value_size(klass)) >= len) return len;
				pos += az_class_value_size(klass);
			}
			break;
		default:
			fprintf(stderr, "Invalid argument signature: %d\n", bci.args);
			return len;
	}
	return pos;
}

static const unsigned char *
print_PUSH_IMMEDIATE (const unsigned char *ip)
{
	unsigned int type;
	AZClass *klass;
	const unsigned char *str;
	type = ip[1];
	ip += 2;
	if (type) {
		klass = az_type_get_class (type);
		str = klass->name;
		ip += az_class_value_size(klass);
	} else {
		str = (const unsigned char *) "null";
	}
	/* fixme: Print value */
	fprintf (stdout, "PUSH_IMMEDIATE %s\n", str);
	return ip;
}

static const unsigned char *
print_TYPE_EQUALS (const unsigned char *ip)
{
	unsigned int type;
	memcpy (&type, ip + 1, 4);
	fprintf (stdout, "IS_TYPE %s\n", az_type_get_class (type)->name);
	return ip + 5;
}

static const unsigned char *
print_0x00 (const unsigned char *ip)
{
	switch (ip[0]) {
	case NOP:
		fprintf (stdout, "NOP\n");
		break;
	}
	return ip + 4;
}

static const unsigned char *
print_NEW_ARRAY (const unsigned char *ip)
{
	fprintf (stdout, "NEW_ARRAY\n");
	return ip + 1;
}

static const unsigned char *
print_LOAD_ARRAY_ELEMENT (const unsigned char *ip)
{
#if 1
	fprintf (stdout, "LOAD_ARRAY_ELEMENT\n");
	return ip + 1;
#else
	unsigned int ic, ra, rb, rd;
	ic = ip[0];
	ra = ip[1];
	rb = ip[2];
	rd = ip[3];
	fprintf (stdout, "LOAD_ARRAY_ELEMENT %d.ARRAY[%d.U32] -> %d\n", ra, rb, rd);
	return ip + 4;
#endif
}

static const unsigned char *
print_WRITE_ARRAY_ELEMENT (const unsigned char *ip)
{
#if 1
	fprintf (stdout, "WRITE_ARRAY_ELEMENT\n");
	return ip + 1;
#else
	unsigned int ic, ra, rb, rc;
	ic = ip[0];
	ra = ip[1];
	rb = ip[2];
	rc = ip[3];
	fprintf (stdout, "WRITE_ARRAY_ELEMENT %d.ARRAY[%d.U32] <- %d\n", ra, rb, rc);
	return ip + 4;
#endif
}

void
print_bytecode (AZOProgram *prog)
{
	const unsigned char *code = prog->tcode;
	unsigned int length = prog->tcode_length;
	const unsigned char *end, *ip;
	ip = code;
	end = code + length;
	while (ip < end) {
		fprintf (stdout, "%04X ", (int) (ip - code));
		switch (*ip & 127) {
		case NOP:
			ip = print_0x00 (ip);
			break;
		case AZO_TC_POP:
			fprintf (stdout, "POP\n");
			ip += 1;
			break;
		case PUSH_IMMEDIATE:
			ip = print_PUSH_IMMEDIATE (ip);
			break;
		case AZO_TC_TYPE_EQUALS_IMMEDIATE:
			ip = print_TYPE_EQUALS (ip);
			break;

		case NEW_ARRAY:
			ip = print_NEW_ARRAY (ip);
			break;
		case LOAD_ARRAY_ELEMENT:
			ip = print_LOAD_ARRAY_ELEMENT (ip);
			break;
		case WRITE_ARRAY_ELEMENT:
			ip = print_WRITE_ARRAY_ELEMENT (ip);
			break;
		default:
			fprintf (stdout, "UNKNOWN %08X", *ip);
			ip += 1;
			break;
		}
	}
}

