#define __AZO_BYTECODE_C__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <az/types.h>

#include "interpreter.h"

#include <azo/bytecode.h>

static const unsigned char *
az_type_get_name (unsigned int type)
{
	AZClass *klass = az_type_get_class (type);
	return klass->name;
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
	case END:
		fprintf (stdout, "END\n");
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
		case END:
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

