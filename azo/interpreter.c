#define __AZO_INTERPRETER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define debug 1

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <arikkei/arikkei-iolib.h>

#include <az/classes/active-object.h>
#include <az/boxed-interface.h>
#include <az/class.h>
#include <az/field.h>
#include <az/collections/list.h>
#include <az/primitives.h>
#include <az/string.h>
#include <az/function.h>
#include <az/reference.h>
#include <az/value.h>
#include <az/classes/value-array-ref.h>

#include <azo/bytecode.h>
#include <azo/compiled-function.h>
#include <azo/private.h>

#include <azo/interpreter.h>

AZOInterpreter *
azo_interpreter_new (AZOContext *ctx)
{
	AZOInterpreter *intr = (AZOInterpreter *) malloc (sizeof (AZOInterpreter));
	memset (intr, 0, sizeof (AZOInterpreter));

	intr->size_frames = 64;
	intr->frames = (unsigned int *) malloc (intr->size_frames * 4);
	intr->frames[0] = 0;
	intr->n_frames = 1;
	intr->ctx = ctx;

	az_instance_init_by_type (&intr->stack, AZO_TYPE_STACK);
	az_instance_init_by_type (&intr->exc, AZO_TYPE_EXCEPTION);

	return intr;
}

void
interpreter_delete (AZOInterpreter *intr)
{
	az_instance_finalize_by_type (&intr->stack, AZO_TYPE_STACK);
	az_instance_finalize_by_type (&intr->exc, AZO_TYPE_EXCEPTION);
	free (intr->frames);
	free (intr);
}

void
azo_intepreter_push_instance (AZOInterpreter *intr, const AZImplementation *impl, void *inst)
{
	azo_stack_push_instance (&intr->stack, impl, inst);
}

void
azo_intepreter_push_value (AZOInterpreter *intr, const AZImplementation *impl, const AZValue *val)
{
	azo_stack_push_value (&intr->stack, impl, val);
}

void
azo_intepreter_push_values (AZOInterpreter *intr, const AZImplementation **impls, const AZValue **vals, unsigned int n_vals)
{
	unsigned int i;
	for (i = 0; i < n_vals; i++) {
		azo_stack_push_value (&intr->stack, impls[i], vals[i]);
	}
}

void
azo_interpreter_push_frame (AZOInterpreter *intr, unsigned int n_elements)
{
	if (intr->n_frames >= intr->size_frames) {
		intr->size_frames = intr->size_frames << 1;
		intr->frames = ( unsigned int *) realloc (intr->frames, intr->size_frames * 4);
	}
	intr->frames[intr->n_frames++] = intr->stack.length - n_elements;
}

void
azo_interpreter_pop_frame (AZOInterpreter *intr)
{
	intr->n_frames -= 1;
}

void
azo_interpreter_clear_frame (AZOInterpreter *intr)
{
	azo_stack_pop (&intr->stack, intr->stack.length - intr->frames[intr->n_frames - 1]);
}

void
azo_interpreter_restore_frame (AZOInterpreter *intr, unsigned int frame)
{
	while (intr->n_frames > frame) {
		assert (intr->stack.length >= intr->frames[intr->n_frames - 1]);
		azo_interpreter_clear_frame (intr);
		azo_interpreter_pop_frame (intr);
	}
}

/* Convert value to u32, return 1 on success, fill exception data and return 0 on failure */

static unsigned int
convert_to_u32 (AZOInterpreter *intr, unsigned int *dst, const AZImplementation *impl, void *val, unsigned int ipc)
{
	unsigned int overflow = 0;
	unsigned int negative = 0;
	unsigned int fractional = 0;
	switch (AZ_IMPL_TYPE(impl)) {
	case AZ_TYPE_UINT8:
		*dst = (unsigned int) *((unsigned char *) val);
		return 1;
	case AZ_TYPE_UINT16:
		*dst = (unsigned int) *((unsigned short *) val);
		return 1;
	case AZ_TYPE_UINT32:
		*dst = (unsigned int) *((unsigned int *) val);
		return 1;
	case AZ_TYPE_UINT64:
		if (*((unsigned long long *) val) >= 0x100000000ULL) {
			overflow = 1;
			break;
		}
		*dst = (unsigned int) *((unsigned long long *) val);
		return 1;
	case AZ_TYPE_INT8:
		if (*((char *) val) < 0) {
			negative = 1;
			break;
		}
		*dst = (unsigned int) *((char *) val);
		return 1;
	case AZ_TYPE_INT16:
		if (*((short *) val) < 0) {
			negative = 1;
			break;
		}
		*dst = (unsigned int) *((short *) val);
		return 1;
	case AZ_TYPE_INT32:
		if (*((int *) val) < 0) {
			negative = 1;
			break;
		}
		*dst = (unsigned int) *((int *) val);
		return 1;
	case AZ_TYPE_INT64:
		if (*((long long *) val) < 0) {
			negative = 1;
			break;
		}
		if (*((long long *) val) >= 0x100000000LL) {
			overflow = 1;
			break;
		}
		*dst = (unsigned int) *((long long *) val);
		return 1;
	case AZ_TYPE_FLOAT:
		if (*((float *) val) < 0) {
			negative = 1;
			break;
		}
		if (*((float *) val) >= 0x100000000LL) {
			overflow = 1;
			break;
		}
		if (fmod (*((float *) val), 1) != 0) {
			fractional = 1;
			break;
		}
		*dst = (unsigned int) *((long long *) val);
		return 1;
	case AZ_TYPE_DOUBLE:
		if (*((double *) val) < 0) {
			negative = 1;
			break;
		}
		if (*((double *) val) >= 0x100000000LL) {
			overflow = 1;
			break;
		}
		if (fmod (*((double *) val), 1) != 0) {
			fractional = 1;
			break;
		}
		*dst = (unsigned int) *((long long *) val);
		return 1;
	default:
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_CONVERSION, 1UL << AZO_EXCEPTION_INVALID_CONVERSION, ipc);
		return 0;
	}
	if (negative || overflow) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, 1UL << AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, ipc);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_CHANGE_OF_PRECISION, 1UL << AZO_EXCEPTION_CHANGE_OF_PRECISION, ipc);
	}
	return 0;
}

/* New stack methods */

static unsigned int
test_stack_type_exact (AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if ((pos + 1) > intr->stack.length) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (azo_stack_type_bw (&intr->stack, pos) != type) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_is (AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if ((pos + 1) > intr->stack.length) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, ( unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (!az_type_is_a (azo_stack_type_bw (&intr->stack, pos), type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_implements (AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if ((pos + 1) > intr->stack.length) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, ( unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (!az_type_implements (azo_stack_type_bw (&intr->stack, pos), type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_exact_2 (AZOInterpreter *intr, const unsigned char *ip, unsigned int type)
{
	if (intr->stack.length < 2) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (azo_stack_type_bw (&intr->stack, 0) != type) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (azo_stack_type_bw (&intr->stack, 1) != type) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_is_2 (AZOInterpreter *intr, const unsigned char *ip, unsigned int type)
{
	if (intr->stack.length < 2) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (!az_type_is_a (azo_stack_type_bw (&intr->stack, 0), type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	if (!az_type_is_a (azo_stack_type_bw (&intr->stack, 1), type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_underflow (AZOInterpreter *intr, const unsigned char *ip, unsigned int n_values)
{
	if (intr->stack.length < n_values) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_overflow (AZOInterpreter *intr, const unsigned char *ip, unsigned int n_values)
{
	if ((intr->stack.length + n_values) > 65536) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_OVERFLOW, 1UL << AZO_EXCEPTION_STACK_OVERFLOW, (unsigned int) (ip - intr->prog->tcode));
		return 0;
	}
	return 1;
}

static const unsigned char *
interpret_EXCEPTION (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if ((*ip & 0x7f) == AZO_TC_EXCEPTION_IF) {
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
		if(!azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	} else if ((*ip & 0x7f) == AZO_TC_EXCEPTION_IF_NOT) {
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
		if (azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	}
	azo_exception_set (&intr->exc, type, 1U << type, (unsigned int) (ip - intr->prog->tcode));
	return NULL;
}

static const unsigned char *
interpret_DEBUG (AZOInterpreter *intr, AZOProgram *prog, const unsigned char *ip)
{
	uint32_t op, len = 5;
	memcpy (&op, ip + 1, 4);
	if (op == 1) {
		fprintf (stderr, "Debug: IPC = %u\n", (unsigned int) (ip - intr->prog->tcode));
		azo_intepreter_print_stack (intr, stderr);
		// azo_stack_print_contents (&intr->stack, 0, intr->stack.length, stderr);
	} else if (op == 2) {
		uint32_t loc;
		memcpy (&loc, ip + 5, 4);
		fprintf (stderr, "Debug: %s\n", prog->values[loc].v.string->str);
		len += 4;
	}
	return ip + len;
}

static const unsigned char *
interpret_PUSH_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos)) return NULL;
	azo_interpreter_push_frame (intr, pos);
	return ip + 5;
}

static const unsigned char *
interpret_POP_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!intr->n_frames) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (intr->frames[intr->n_frames - 1] > intr->stack.length) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	azo_interpreter_pop_frame (intr);
	return ip + 1;
}

static const unsigned char *
interpret_POP (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int n_values;
	memcpy (&n_values, ip + 1, 4);
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 1)) return NULL;
	azo_stack_pop (&intr->stack, n_values);
	return ip + 5;
}

static const unsigned char *
interpret_REMOVE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int first, n_values;
	memcpy (&first, ip + 1, 4);
	memcpy (&n_values, ip + 5, 4);
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, first + n_values)) return NULL;
	azo_stack_remove (&intr->stack, intr->stack.length - (first + n_values), n_values);
	return ip + 9;
}

static const unsigned char *
interpret_PUSH_EMPTY (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if (!test_stack_overflow (intr, ip, 1)) return NULL;
	if (type) {
		AZClass *klass = az_type_get_class (type);
		if (ip[0] & AZO_TC_CHECK_ARGS) {
			if (AZ_CLASS_FLAGS(klass) & AZ_FLAG_ABSTRACT) {
				azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
				return NULL;
			}
		}
		azo_stack_push_value_default (&intr->stack, &klass->impl);
	} else {
		azo_stack_push_value_default (&intr->stack, NULL);
	}
	return ip + 5;
}

static const unsigned char *
interpret_PUSH_IMMEDIATE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	AZClass *klass;
	type = ip[1];
	if (!test_stack_overflow (intr, ip, 1)) return NULL;
	/* Type has to be primitive or NULL */
	if (type && !AZ_TYPE_IS_PRIMITIVE (type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	if (type) {
		klass = az_type_get_class (type);
		azo_stack_push_value (&intr->stack, &klass->impl, ip + 2);
		return ip + 2 + az_class_value_size(klass);
	} else {
		azo_stack_push_value (&intr->stack, NULL, NULL);
		return ip + 2;
	}
}

static const unsigned char *
interpret_PUSH_VALUE (AZOInterpreter *intr, AZOProgram *prog, const unsigned char *ip)
{
	uint32_t loc;
	memcpy (&loc, ip + 1, 4);
	if (!test_stack_overflow (intr, ip, 1)) return NULL;
	azo_stack_push_value (&intr->stack, prog->values[loc].impl, &prog->values[loc].v);
	return ip + 5;
}

static const unsigned char *
interpret_DUPLICATE (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos)) return NULL;
	if (!test_stack_overflow (intr, ip, 1)) return NULL;
	azo_stack_duplicate (&intr->stack, intr->stack.length - 1 - pos);
	return ip + 5;
}

static const unsigned char *
interpret_DUPLICATE_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if ((intr->frames[intr->n_frames - 1] + pos) >= intr->stack.length) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	if (!test_stack_overflow (intr, ip, 1)) return NULL;
	azo_stack_duplicate (&intr->stack, intr->frames[intr->n_frames - 1] + pos);
	return ip + 5;
}

static const unsigned char *
interpret_EXCHANGE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos;
	memcpy (&pos, ip + 1, 4);
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos)) return NULL;
	azo_stack_exchange (&intr->stack, intr->stack.length - 1 - pos);
	return ip + 5;
}

static const unsigned char *
interpret_EXCHANGE_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos;
	memcpy (&pos, ip + 1, 4);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if ((intr->frames[intr->n_frames - 1] + pos) >= intr->stack.length) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	azo_stack_exchange (&intr->stack, intr->frames[intr->n_frames - 1] + pos);
	return ip + 5;
}

static const unsigned char *
interpret_TYPE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos, result;
	pos = ip[1];
	if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_UINT32)) return NULL;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos + 1)) return NULL;
	uint32_t type = azo_stack_uint32_bw(&intr->stack, 0);
	switch (ip[0] & 127) {
	case AZO_TC_TYPE_EQUALS:
		result = (azo_stack_type_bw (&intr->stack, pos) == type);
		break;
	case AZO_TC_TYPE_IS:
		result = azo_stack_type_bw (&intr->stack, pos) ? az_type_is_a (azo_stack_type_bw (&intr->stack, pos), type) : 0;
		break;
	case AZO_TC_TYPE_IS_SUPER:
		result = (type) ? az_type_is_a (type, azo_stack_type_bw (&intr->stack, pos)) : 0;
		break;
	case AZO_TC_TYPE_IMPLEMENTS:
		result = azo_stack_type_bw (&intr->stack, pos) ? az_type_implements (azo_stack_type_bw (&intr->stack, pos), type) : 0;
		break;
	}
	azo_stack_pop (&intr->stack, 1);
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE(AZ_TYPE_BOOLEAN), &result);
	return ip + 2;
}

static const unsigned char *
interpret_TYPE_IMMEDIATE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos, type, result;
	pos = ip[1];
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos + 1)) return NULL;
	memcpy (&type, ip + 2, 4);
	switch (ip[0] & 127) {
	case AZO_TC_TYPE_EQUALS_IMMEDIATE:
		result = (azo_stack_type_bw (&intr->stack, pos) == type);
		break;
	case AZO_TC_TYPE_IS_IMMEDIATE:
		result = azo_stack_type_bw (&intr->stack, pos) ? az_type_is_a (azo_stack_type_bw (&intr->stack, pos), type) : 0;
		break;
	case AZO_TC_TYPE_IS_SUPER_IMMEDIATE:
		result = az_type_is_a (type, azo_stack_type_bw (&intr->stack, pos));
		break;
	case AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE:
		result = azo_stack_type_bw (&intr->stack, pos) ? az_type_implements (azo_stack_type_bw (&intr->stack, pos), type) : 0;
		break;
	}
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE(AZ_TYPE_BOOLEAN), &result);
	return ip + 6;
}

static const unsigned char *
interpret_TYPE_OF (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	unsigned int pos;
	pos = ip[1];
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos + 1)) return NULL;
	if ((ip[0] & 127) == TYPE_OF) {
		type = azo_stack_type_bw (&intr->stack, pos);
		azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE(AZ_TYPE_UINT32), &type);
	} else {
		if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_CLASS)) return NULL;
		AZClass *klass = (AZClass *) azo_stack_instance_bw (&intr->stack, 0);
		azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE(AZ_TYPE_UINT32), &AZ_CLASS_TYPE(klass));
	}
	return ip + 2;
}

static const unsigned char *
interpret_JMP_32 (AZOInterpreter *intr, const unsigned char *ip)
{
	int32_t raddr;
	memcpy (&raddr, ip + 1, 4);
	return ip + 5 + raddr;
}

static const unsigned char *
interpret_JMP_32_COND (AZOInterpreter *intr, const unsigned char *ip)
{
	int raddr;
	const unsigned char *dst;
	memcpy (&raddr, ip + 1, 4);
	dst = ip + 5;
	switch (ip[0] & 0x7f) {
	case JMP_32_IF:
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
		if (azo_stack_boolean_bw(&intr->stack, 0)) dst += raddr;
		break;
	case JMP_32_IF_NOT:
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
		if (!azo_stack_boolean_bw(&intr->stack, 0)) dst += raddr;
		break;
	case JMP_32_IF_ZERO:
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_INT32)) return NULL;
		if (azo_stack_int32_bw(&intr->stack, 0) == 0) dst += raddr;
		break;
	case JMP_32_IF_POSITIVE:
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_INT32)) return NULL;
		if (azo_stack_int32_bw(&intr->stack, 0) > 0) dst += raddr;
		break;
	case JMP_32_IF_NEGATIVE:
		if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_type_exact (intr, ip, 0, AZ_TYPE_INT32)) return NULL;
		if (azo_stack_int32_bw(&intr->stack, 0) < 0) dst += raddr;
		break;
	}
	azo_stack_pop (&intr->stack, 1);
	return dst;
}

static const unsigned char *
interpret_POINTER_TO_U64 (AZOInterpreter *intr, const unsigned char *ip)
{
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_POINTER)) return NULL;
	}
	// fixme: Reinterpret
	uint64_t val = (uint64_t) azo_stack_pointer_bw(&intr->stack, 0);
	azo_stack_pop (&intr->stack, 1);
	azo_stack_push_value (&intr->stack, &az_type_get_class (AZ_TYPE_UINT64)->impl, &val);
	return ip + 1;
}

static const unsigned char *
interpret_U64_TO_POINTER (AZOInterpreter *intr, const unsigned char *ip)
{
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_UINT64)) return NULL;
	}
	uint64_t val = (uint64_t) azo_stack_pointer_bw(&intr->stack, 0);
	azo_stack_pop (&intr->stack, 1);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_POINTER), &val);
	return ip + 1;
}

static const unsigned char *
interpret_PROMOTE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos, result;
	pos = ip[1];
	if (*ip & AZO_TC_CHECK_ARGS) {
	}
	unsigned int type = azo_stack_uint32_bw(&intr->stack, 0);
	/* fixme: Test clamped/rounded */
	if (!azo_stack_convert_bw (&intr->stack, pos, type)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_CONVERSION, 1UL << AZO_EXCEPTION_INVALID_CONVERSION, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_EQUAL_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type, result;
	const AZValue *lhs, *rhs;
	type = ip[1];
	if (AZ_TYPE_IS_PRIMITIVE (type)) {
		if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
		lhs = azo_stack_value_bw(&intr->stack, 1);
		rhs = azo_stack_value_bw(&intr->stack, 0);
		switch (type) {
		case AZ_TYPE_BOOLEAN:
		case AZ_TYPE_INT32:
		case AZ_TYPE_UINT32:
			result = (lhs->uint32_v == rhs->uint32_v);
			break;
		case AZ_TYPE_INT8:
		case AZ_TYPE_UINT8:
			result = (lhs->uint8_v == rhs->uint8_v);
			break;
		case AZ_TYPE_INT16:
		case AZ_TYPE_UINT16:
			result = (lhs->uint16_v == rhs->uint16_v);
			break;
		case AZ_TYPE_INT64:
		case AZ_TYPE_UINT64:
		case AZ_TYPE_POINTER:
			result = (lhs->uint64_v == rhs->uint64_v);
			break;
		case AZ_TYPE_FLOAT:
			result = *((float *) lhs) == *((float *) rhs);
			break;
		case AZ_TYPE_DOUBLE:
			result = *((double *) lhs) == *((double *) rhs);
			break;
		case AZ_TYPE_COMPLEX_FLOAT:
			result = (*((float *) lhs) == *((float *) rhs)) && (*((float *) lhs + 1) == *((float *) rhs + 1));
			break;
		case AZ_TYPE_COMPLEX_DOUBLE:
			result = (*((double *) lhs) == *((double *) rhs)) && (*((double *) lhs + 1) == *((double *) rhs + 1));
			break;
		}
	} else if (type == AZ_TYPE_BLOCK) {
		if (!test_stack_type_is_2 (intr, ip, type)) return NULL;
		lhs = azo_stack_value_bw (&intr->stack, 1);
		rhs = azo_stack_value_bw (&intr->stack, 0);
		result = *((unsigned long long *) lhs) == *((unsigned long long *) rhs);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_BOOLEAN), &result);
	return ip + 2;
}

static const unsigned char *
interpret_EQUAL (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type, result;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	if (AZ_TYPE_IS_PRIMITIVE (type)) {
		if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
		lhs = azo_stack_value_bw (&intr->stack, 1);
		rhs = azo_stack_value_bw (&intr->stack, 0);
		switch (type) {
		case AZ_TYPE_BOOLEAN:
		case AZ_TYPE_INT32:
		case AZ_TYPE_UINT32:
			result = *((unsigned int *) lhs) == *((unsigned int *) rhs);
			break;
		case AZ_TYPE_INT8:
		case AZ_TYPE_UINT8:
			result = *((unsigned char *) lhs) == *((unsigned char *) rhs);
			break;
		case AZ_TYPE_INT16:
		case AZ_TYPE_UINT16:
			result = *((unsigned short *) lhs) == *((unsigned short *) rhs);
			break;
		case AZ_TYPE_INT64:
		case AZ_TYPE_UINT64:
		case AZ_TYPE_POINTER:
			result = *((unsigned long long *) lhs) == *((unsigned long long *) rhs);
			break;
		case AZ_TYPE_FLOAT:
			result = *((float *) lhs) == *((float *) rhs);
			break;
		case AZ_TYPE_DOUBLE:
			result = *((double *) lhs) == *((double *) rhs);
			break;
		case AZ_TYPE_COMPLEX_FLOAT:
			result = (*((float *) lhs) == *((float *) rhs)) && (*((float *) lhs + 1) == *((float *) rhs + 1));
			break;
		case AZ_TYPE_COMPLEX_DOUBLE:
			result = (*((double *) lhs) == *((double *) rhs)) && (*((double *) lhs + 1) == *((double *) rhs + 1));
			break;
		}
	} else if (type == AZ_TYPE_BLOCK) {
		if (!test_stack_type_is_2 (intr, ip, type)) return NULL;
		lhs = azo_stack_value_bw (&intr->stack, 1);
		rhs = azo_stack_value_bw (&intr->stack, 0);
		result = *((unsigned long long *) lhs) == *((unsigned long long *) rhs);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_BOOLEAN), &result);
	return ip + 1;
}

static const unsigned char *
interpret_COMPARE_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const AZValue *lhs, *rhs;
	int result;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	switch (type) {
	case AZ_TYPE_INT8:
		result = (lhs->int8_v < rhs->int8_v) ? -1 : (lhs->int8_v > rhs->int8_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT8:
		result = (lhs->uint8_v < rhs->uint8_v) ? -1 : (lhs->uint8_v > rhs->uint8_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT16:
		result = (lhs->int16_v < rhs->int16_v) ? -1 : (lhs->int16_v > rhs->int16_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT16:
		result = (lhs->uint16_v < rhs->uint16_v) ? -1 : (lhs->uint16_v > rhs->uint16_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT32:
		result = (lhs->int32_v < rhs->int32_v) ? -1 : (lhs->int32_v > rhs->int32_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT32:
		result = (lhs->uint32_v < rhs->uint32_v) ? -1 : (lhs->uint32_v > rhs->uint32_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT64:
		result = (lhs->int64_v < rhs->int64_v) ? -1 : (lhs->int64_v > rhs->int64_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT64:
		result = (lhs->uint64_v < rhs->uint64_v) ? -1 : (lhs->uint64_v > rhs->uint64_v) ? 1 : 0;
		break;
	case AZ_TYPE_FLOAT:
		result = (lhs->float_v < rhs->float_v) ? -1 : (lhs->float_v > rhs->float_v) ? 1 : 0;
		break;
	case AZ_TYPE_DOUBLE:
		result = (lhs->double_v < rhs->double_v) ? -1 : (lhs->double_v > rhs->double_v) ? 1 : 0;
		break;
	default:
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_INT32), &result);
	return ip + 2;
}

static const unsigned char *
interpret_COMPARE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const AZValue *lhs, *rhs;
	int result;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	switch (type) {
	case AZ_TYPE_INT8:
		result = (lhs->int8_v < rhs->int8_v) ? -1 : (lhs->int8_v > rhs->int8_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT8:
		result = (lhs->uint8_v < rhs->uint8_v) ? -1 : (lhs->uint8_v > rhs->uint8_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT16:
		result = (lhs->int16_v < rhs->int16_v) ? -1 : (lhs->int16_v > rhs->int16_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT16:
		result = (lhs->uint16_v < rhs->uint16_v) ? -1 : (lhs->uint16_v > rhs->uint16_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT32:
		result = (lhs->int32_v < rhs->int32_v) ? -1 : (lhs->int32_v > rhs->int32_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT32:
		result = (lhs->uint32_v < rhs->uint32_v) ? -1 : (lhs->uint32_v > rhs->uint32_v) ? 1 : 0;
		break;
	case AZ_TYPE_INT64:
		result = (lhs->int64_v < rhs->int64_v) ? -1 : (lhs->int64_v > rhs->int64_v) ? 1 : 0;
		break;
	case AZ_TYPE_UINT64:
		result = (lhs->uint64_v < rhs->uint64_v) ? -1 : (lhs->uint64_v > rhs->uint64_v) ? 1 : 0;
		break;
	case AZ_TYPE_FLOAT:
		result = (lhs->float_v < rhs->float_v) ? -1 : (lhs->float_v > rhs->float_v) ? 1 : 0;
		break;
	case AZ_TYPE_DOUBLE:
		result = (lhs->double_v < rhs->double_v) ? -1 : (lhs->double_v > rhs->double_v) ? 1 : 0;
		break;
	default:
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_INT32), &result);
	return ip + 1;
}

static const unsigned char *
interpret_LOGICAL_NOT (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *val;
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
	}
	val = azo_stack_value_bw (&intr->stack, 0);
	val->boolean_v = !val->boolean_v;
	return ip + 1;
}

static const unsigned char *
interpret_NEGATE (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *val;
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!AZ_TYPE_IS_SIGNED (type) && (type != AZ_TYPE_COMPLEX_FLOAT) && (type != AZ_TYPE_COMPLEX_DOUBLE)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	val = azo_stack_value_bw (&intr->stack, 0);
	switch (type) {
	case AZ_TYPE_INT8:
		val->int8_v = -val->int8_v;
		break;
	case AZ_TYPE_INT16:
		val->int16_v = -val->int16_v;
		break;
	case AZ_TYPE_INT32:
		val->int32_v = -val->int32_v;
		break;
	case AZ_TYPE_INT64:
		val->int64_v = -val->int64_v;
		break;
	case AZ_TYPE_FLOAT:
		val->float_v = -val->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		val->double_v = -val->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		val->cfloat_v.r = -val->cfloat_v.r;
		val->cfloat_v.i = -val->cfloat_v.i;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		val->cdouble_v.r = -val->cdouble_v.r;
		val->cdouble_v.i = -val->cdouble_v.i;
		break;
	}
	return ip + 1;
}

static const unsigned char *
interpret_CONJUGATE (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *val;
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	if (*ip & AZO_TC_CHECK_ARGS) {
		if ((type != AZ_TYPE_COMPLEX_FLOAT) && (type != AZ_TYPE_COMPLEX_DOUBLE)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	val = azo_stack_value_bw (&intr->stack, 0);
	switch (type) {
	case AZ_TYPE_COMPLEX_FLOAT:
		val->cfloat_v.i = -val->cfloat_v.i;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		val->cdouble_v.i = -val->cdouble_v.i;
		break;
	}
	return ip + 1;
}

static const unsigned char *
interpret_BITWISE_NOT (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *val;
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!AZ_TYPE_IS_INTEGRAL (type)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	val = azo_stack_value_bw (&intr->stack, 0);
	val->uint64_v = ~val->int64_v;
	return ip + 1;
}

static const unsigned char *
interpret_LOGICAL_BINARY (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *lhs, *rhs;
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_BOOLEAN)) return NULL;
		if (!test_stack_type_exact (intr, ip, 1, AZ_TYPE_BOOLEAN)) return NULL;
	}
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if ((*ip & 127) == AZO_TC_LOGICAL_AND) {
		lhs->boolean_v = lhs->boolean_v && rhs->boolean_v;
	} else {
		lhs->boolean_v = lhs->boolean_v || rhs->boolean_v;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_ADD_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) + *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) + *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) + *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) + *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) + *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) + *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		*((float *) lhs) = *((float *) lhs) + *((float *) rhs);
		*((float *) lhs + 1) = *((float *) lhs + 1) + *((float *) rhs + 1);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) + *((double *) rhs);
		*((double *) lhs + 1) = *((double *) lhs + 1) + *((double *) rhs + 1);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_SUBTRACT_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) - *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) - *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) - *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) - *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) - *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) - *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		*((float *) lhs) = *((float *) lhs) - *((float *) rhs);
		*((float *) lhs + 1) = *((float *) lhs - 1) + *((float *) rhs + 1);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) - *((double *) rhs);
		*((double *) lhs + 1) = *((double *) lhs - 1) + *((double *) rhs + 1);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_MULTIPLY_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) * *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) * *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) * *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) * *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) * *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) * *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		float r = *((float *) lhs) * *((float *) rhs) - *((float *) lhs + 1) * *((float *) rhs + 1);
		float i = *((float *) lhs) * *((float *) rhs + 1) + *((float *) lhs + 1) * *((float *) rhs);
		*((float *) lhs) = r;
		*((float *) lhs + 1) = i;
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		double r = *((double *) lhs) * *((double *) rhs) - *((double *) lhs + 1) * *((double *) rhs + 1);
		double i = *((double *) lhs) * *((double *) rhs + 1) + *((double *) lhs + 1) * *((double *) rhs);
		*((double *) lhs) = r;
		*((double *) lhs + 1) = i;
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_DIVIDE_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) / *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) / *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) / *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) / *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) / *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) / *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		float a = *((float *) lhs);
		float b = *((float *) lhs + 1);
		float c = *((float *) rhs);
		float d = *((float *) rhs + 1);
		*((float *) lhs) = (a * c + b * d) / (c * c + d * d);
		*((float *) lhs + 1) = (b * c - a * d) / (c * c + d * d);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		double a = *((double *) lhs);
		double b = *((double *) lhs + 1);
		double c = *((double *) rhs);
		double d = *((double *) rhs + 1);
		*((double *) lhs) = (a * c + b * d) / (c * c + d * d);
		*((double *) lhs + 1) = (b * c - a * d) / (c * c + d * d);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_MODULO_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) % *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) % *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) % *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) % *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = fmodf (*((float *) lhs), *((float *) rhs));
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = fmod (*((double *) lhs), *((double *) rhs));
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

static const unsigned char *
interpret_ADD (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) + *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) + *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) + *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) + *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) + *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) + *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		*((float *) lhs) = *((float *) lhs) + *((float *) rhs);
		*((float *) lhs + 1) = *((float *) lhs + 1) + *((float *) rhs + 1);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) + *((double *) rhs);
		*((double *) lhs + 1) = *((double *) lhs + 1) + *((double *) rhs + 1);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_SUBTRACT (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) - *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) - *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) - *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) - *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) - *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) - *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		*((float *) lhs) = *((float *) lhs) - *((float *) rhs);
		*((float *) lhs + 1) = *((float *) lhs - 1) + *((float *) rhs + 1);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) - *((double *) rhs);
		*((double *) lhs + 1) = *((double *) lhs - 1) + *((double *) rhs + 1);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_MULTIPLY (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) * *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) * *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) * *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) * *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) * *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) * *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		float r = *((float *) lhs) * *((float *) rhs) - *((float *) lhs + 1) * *((float *) rhs + 1);
		float i = *((float *) lhs) * *((float *) rhs + 1) + *((float *) lhs + 1) * *((float *) rhs);
		*((float *) lhs) = r;
		*((float *) lhs + 1) = i;
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		double r = *((double *) lhs) * *((double *) rhs) - *((double *) lhs + 1) * *((double *) rhs + 1);
		double i = *((double *) lhs) * *((double *) rhs + 1) + *((double *) lhs + 1) * *((double *) rhs);
		*((double *) lhs) = r;
		*((double *) lhs + 1) = i;
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_DIVIDE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) / *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) / *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) / *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) / *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = *((float *) lhs) / *((float *) rhs);
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = *((double *) lhs) / *((double *) rhs);
	} else if (type == AZ_TYPE_COMPLEX_FLOAT) {
		float a = *((float *) lhs);
		float b = *((float *) lhs + 1);
		float c = *((float *) rhs);
		float d = *((float *) rhs + 1);
		*((float *) lhs) = (a * c + b * d) / (c * c + d * d);
		*((float *) lhs + 1) = (b * c - a * d) / (c * c + d * d);
	} else if (type == AZ_TYPE_COMPLEX_DOUBLE) {
		double a = *((double *) lhs);
		double b = *((double *) lhs + 1);
		double c = *((double *) rhs);
		double d = *((double *) rhs + 1);
		*((double *) lhs) = (a * c + b * d) / (c * c + d * d);
		*((double *) lhs + 1) = (b * c - a * d) / (c * c + d * d);
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_MODULO (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	if ((ip[0] & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, 2)) return NULL;
	type = azo_stack_type_bw (&intr->stack, 0);
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT32) {
		*((int32_t *) lhs) = *((int32_t *) lhs) % *((int32_t *) rhs);
	} else if (type == AZ_TYPE_UINT32) {
		*((uint32_t *) lhs) = *((uint32_t *) lhs) % *((uint32_t *) rhs);
	} else if (type == AZ_TYPE_INT64) {
		*((int64_t *) lhs) = *((int64_t *) lhs) % *((int64_t *) rhs);
	} else if (type == AZ_TYPE_UINT64) {
		*((uint64_t *) lhs) = *((uint64_t *) lhs) % *((uint64_t *) rhs);
	} else if (type == AZ_TYPE_FLOAT) {
		*((float *) lhs) = fmodf (*((float *) lhs), *((float *) rhs));
	} else if (type == AZ_TYPE_DOUBLE) {
		*((double *) lhs) = fmod (*((double *) lhs), *((double *) rhs));
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_MIN_MAX_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	const void *lhs, *rhs;
	unsigned int remove;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	lhs = azo_stack_value_bw (&intr->stack, 1);
	rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT8) {
		remove = (*((int8_t *) lhs) > *((int8_t *) rhs));
	} else if (type == AZ_TYPE_UINT8) {
		remove = (*((uint8_t *) lhs) > *((uint8_t *) rhs));
	} else if (type == AZ_TYPE_INT16) {
		remove = (*((int16_t *) lhs) > *((int16_t *) rhs));
	} else if (type == AZ_TYPE_UINT16) {
		remove = (*((uint16_t *) lhs) > *((uint16_t *) rhs));
	} else if (type == AZ_TYPE_INT32) {
		remove = (*((int32_t *) lhs) > *((int32_t *) rhs));
	} else if (type == AZ_TYPE_UINT32) {
		remove = (*((uint32_t *) lhs) > *((uint32_t *) rhs));
	} else if (type == AZ_TYPE_INT64) {
		remove = (*((int64_t *) lhs) > *((int64_t *) rhs));
	} else if (type == AZ_TYPE_UINT64) {
		remove = (*((uint64_t *) lhs) > *((uint64_t *) rhs));
	} else if (type == AZ_TYPE_FLOAT) {
		remove = (*((float *) lhs) > *((float *) rhs));
	} else if (type == AZ_TYPE_DOUBLE) {
		remove = (*((double *) lhs) > *((double *) rhs));
	} else {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	if (ip[0] == MAX_TYPED) remove = !remove;
	azo_stack_remove (&intr->stack, intr->stack.length - 1 - remove, 1);
	return ip + 2;
}

static const unsigned char *
interpret_GET_INTERFACE_IMMEDIATE (AZOInterpreter *intr, const unsigned char *ip)
{
	const AZImplementation *impl;
	void *inst;
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 1)) return NULL;
		if (azo_stack_type_bw (&intr->stack, 0) <= AZ_TYPE_ANY) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (az_type_is_a (azo_stack_type_bw (&intr->stack, 0), AZ_TYPE_STRUCT)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (!az_type_is_a (type, AZ_TYPE_INTERFACE)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	impl = az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0), type, &inst);
	azo_stack_push_value (&intr->stack, impl, &inst);
	return ip + 5;
}

/*
 * INVOKE N_ARGS
 *
 * [func : this, arg1...]
 * [func : this, arg1..., result]
 */
static const unsigned char *
interpret_INVOKE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos = ip[1];
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, pos)) return NULL;
	}
	AZFunctionInstance *func_inst;
	const AZFunctionImplementation *func_impl = (const AZFunctionImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, pos), azo_stack_instance_bw (&intr->stack, pos), AZ_TYPE_FUNCTION, (void **) &func_inst);
	if (!func_impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	const AZFunctionSignature *sig = az_function_get_signature (func_impl, func_inst);
	for (unsigned int i = 0; i < sig->n_args; i++) {
		if (!azo_stack_convert_bw (&intr->stack, sig->n_args - 1 - i, sig->arg_types[i])) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
	}
	intr->vals[0].impl = NULL;
	az_function_invoke (func_impl, func_inst, azo_stack_impls_bw (&intr->stack, sig->n_args - 1), (const AZValue **) azo_stack_values_bw (&intr->stack, sig->n_args - 1), &intr->vals[0].impl, &intr->vals[0].v, NULL);
	azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
	intr->vals[0].impl = NULL;
	return ip + 2;
}

static const unsigned char *
interpret_RETURN (AZOInterpreter *intr, const unsigned char *ip, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size)
{
	if (ret_impl) {
		if (ip[0] & AZO_TC_CHECK_ARGS) {
			if (!test_stack_underflow (intr, ip, 0)) return NULL;
		}
		*ret_impl = az_value_copy_autobox(azo_stack_impl_bw(&intr->stack, 0), ret_val, azo_stack_value_bw(&intr->stack, 0), ret_size);
	}
	return NULL;
}

static const unsigned char *
interpret_BIND (AZOInterpreter *intr, const unsigned char *ip)
{
	AZOCompiledFunction *cfunc;
	uint32_t pos, i;
	memcpy (&pos, ip + 1, 4);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, pos, AZO_TYPE_COMPILED_FUNCTION)) return NULL;
	}
	cfunc = (AZOCompiledFunction *) azo_stack_instance_bw (&intr->stack, pos);
	for (i = 0; i < pos; i++) {
		azo_compiled_function_bind (cfunc, i, azo_stack_impl_bw (&intr->stack, pos - 1 - i), azo_stack_instance_bw (&intr->stack, pos - 1 - i));
	}
	if (pos) azo_stack_pop (&intr->stack, pos);
	return ip + 5;
}

static const unsigned char *
interpret_NEW_ARRAY (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int size;
	AZValueArrayRef *varray;
	AZClass *varray_class;
	if (*ip & AZO_TC_CHECK_ARGS) {
		/* Test stack size */
		if (intr->stack.length < 1) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_STACK_UNDERFLOW, 1UL << AZO_EXCEPTION_STACK_UNDERFLOW, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (!convert_to_u32 (intr, &size, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0), (unsigned int) (ip - intr->prog->tcode))) {
			return NULL;
		}
	} else {
		size = *((unsigned int *) azo_stack_value_bw (&intr->stack, 0));
	}
	azo_stack_pop (&intr->stack, 1);
	varray = az_value_array_ref_new (size);
	varray_class = az_type_get_class (AZ_TYPE_VALUE_ARRAY_REF);
	azo_stack_push_instance (&intr->stack, &varray_class->impl, varray);
	az_reference_unref ((AZReferenceClass *) varray_class, &varray->reference);
	return ip + 1;
}

static const unsigned char *
interpret_LOAD_ARRAY_ELEMENT (AZOInterpreter *intr, const unsigned char *ip)
{
	AZListImplementation *list_impl;
	void *list_inst;
	unsigned int idx;
	AZPackedValue64 val = { 0 };
	if (*ip & AZO_TC_CHECK_ARGS) {
		const AZImplementation *obj_impl = azo_stack_impl_bw (&intr->stack, 1);
		const AZImplementation *idx_impl = azo_stack_impl_bw (&intr->stack, 0);
		if (!obj_impl || !idx_impl) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_NULL_DEREFERENCE, 1UL << AZO_EXCEPTION_NULL_DEREFERENCE, ( unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		list_impl = (AZListImplementation *) az_instance_get_interface (obj_impl, azo_stack_instance_bw (&intr->stack, 1), AZ_TYPE_LIST, (void **) &list_inst);
		if (!list_impl) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (!convert_to_u32 (intr, &idx, idx_impl, azo_stack_value_bw (&intr->stack, 0), (unsigned int) (ip - intr->prog->tcode))) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_CONVERSION, 1UL << AZO_EXCEPTION_INVALID_CONVERSION, ( unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		if (idx >= az_collection_get_size (&list_impl->collection_impl, list_inst)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_OUT_OF_BOUNDS, 1UL << AZO_EXCEPTION_OUT_OF_BOUNDS, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		val.impl = az_list_get_element (list_impl, list_inst, idx, &val.v.value, 64);
		if (!val.impl) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_SYSTEM, 1UL << AZO_EXCEPTION_SYSTEM, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v);
	} else {
		list_impl = (AZListImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 1), azo_stack_instance_bw (&intr->stack, 1), AZ_TYPE_LIST, (void **) &list_inst);
		idx = *((unsigned int *) azo_stack_value_bw (&intr->stack, 0));
		val.impl = az_list_get_element (list_impl, list_inst, idx, &val.v.value, 64);
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v);
	}
	return ip + 1;
}

static const unsigned char *
interpret_WRITE_ARRAY_ELEMENT (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValueArrayRef *varray;
	unsigned int idx;
	AZPackedValue val = { 0 };
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!az_type_is_a (azo_stack_type_bw (&intr->stack, 2), AZ_TYPE_VALUE_ARRAY_REF)) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		varray = *((AZValueArrayRef **) azo_stack_value_bw (&intr->stack, 2));
		if (!convert_to_u32 (intr, &idx, azo_stack_impl_bw (&intr->stack, 1), azo_stack_value_bw (&intr->stack, 1), (unsigned int) (ip - intr->prog->tcode))) {
			return NULL;
		}
		if (idx >= varray->varray.length) {
			azo_exception_set (&intr->exc, AZO_EXCEPTION_OUT_OF_BOUNDS, 1UL << AZO_EXCEPTION_OUT_OF_BOUNDS, (unsigned int) (ip - intr->prog->tcode));
			return NULL;
		}
		az_packed_value_set_from_impl_value (&val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
	} else {
		varray = *((AZValueArrayRef **) azo_stack_value_bw (&intr->stack, 2));
		idx = *((unsigned int *) azo_stack_value_bw (&intr->stack, 1));
		az_packed_value_set_from_impl_value (&val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
	}
	az_value_array_ref_set_element (varray, idx, val.impl, &val.v);
	az_packed_value_clear (&val);
	azo_stack_pop (&intr->stack, 2);
	return ip + 1;
}

static const unsigned char *
interpret_GET_GLOBAL (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	intr->vals[0].impl = NULL;
	if (azo_context_lookup (intr->ctx, key, &intr->vals[0].packed_val)) {
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v.value);
	} else {
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value (&intr->stack, NULL, NULL);
	}
	return ip + 1;
}

/* Instance, String -> Value */

static const unsigned char *
interpret_GET_PROPERTY (AZOInterpreter *intr, const unsigned char *ip)
{
	const AZImplementation *impl;
	void *inst;
	AZString *key;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	/* fixme: Implement automatic unboxing (in stack?) or not? */
	impl = azo_stack_impl_bw (&intr->stack, 1);
	if (!impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_NULL_DEREFERENCE, 1UL << AZO_EXCEPTION_NULL_DEREFERENCE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	inst = azo_stack_instance_bw (&intr->stack, 1);
	if (impl == &AZBoxedInterfaceKlass.klass.impl) {
		az_boxed_interface_unbox(&impl, &inst);
	}
	intr->vals[0].impl = NULL;
	/* Try simple property */
	if (az_instance_get_property_by_key (impl, inst, key->str, &intr->vals[0].impl, &intr->vals[0].v)) {
		/* Ordinary property */
		azo_stack_pop (&intr->stack, 2);
		azo_stack_push_value_transfer (&intr->stack, intr->vals[0].packed_val.impl, &intr->vals[0].packed_val.v);
		return ip + 1;
	}
	/* Try interface */
	if (azo_context_lookup (intr->ctx, key, &intr->vals[0].packed_val) && (AZ_IMPL_TYPE(intr->vals[0].impl) == AZ_TYPE_CLASS)) {
		AZClass *klass = (AZClass *) intr->vals[0].packed_val.v.block;
		if ((AZ_CLASS_FLAGS(klass) & AZ_FLAG_INTERFACE) && az_type_implements (AZ_IMPL_TYPE(impl), AZ_CLASS_TYPE(klass))) {
			/* fixme: Implement nested interfacing (by overriding boxed iface) */
			AZBoxedInterface *boxed = az_boxed_interface_new_from_impl_instance (impl, inst, AZ_CLASS_TYPE(klass));
			azo_stack_pop (&intr->stack, 2);
			azo_stack_push_value_transfer (&intr->stack, (const AZImplementation *) &AZBoxedInterfaceKlass, &boxed);
			return ip + 1;
		}
	}
	/* Try static property */
	if (AZ_IMPL_TYPE(impl) == AZ_TYPE_CLASS) {
		AZClass *klass = (AZClass *) inst;
		const AZClass *sub_class;
		const AZImplementation *prop_impl;
		int idx = az_class_lookup_property (klass, &klass->impl, NULL, key, &sub_class, &prop_impl, NULL);
		if (idx >= 0) {
			AZField *field = &sub_class->props_self[idx];
			if (field->spec == AZ_FIELD_INSTANCE) {
				/* Field is instance */
			} else {
				if (az_instance_get_property_by_id (sub_class, AZ_CLASS_FROM_IMPL(prop_impl), prop_impl, NULL, idx, &intr->vals[0].impl, &intr->vals[0].v.value, 64, NULL)) {
					azo_stack_pop (&intr->stack, 2);
					azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
					return ip + 1;
				} else {
					/* Field is not readable */
				}
			}
		}
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 1;
}

/* Instance, String, Arguments -> Instance, String, Arguments, Function | null */

static const unsigned char *
interpret_GET_FUNCTION (AZOInterpreter *intr, AZOProgram *prog, const unsigned char *ip)
{
	unsigned int n_args = ip[1];
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, n_args + 2)) return NULL;
		if (!test_stack_type_exact (intr, ip, n_args, AZ_TYPE_STRING)) return NULL;
	}
	AZFunctionSignature32 sig;
	sig.n_args = n_args;
	sig.ret_type = AZ_TYPE_ANY;
	for (unsigned int i = 0; i < n_args; i++) {
		if (i >= 32) break;
		sig.arg_types[i] = azo_stack_type_bw (&intr->stack, n_args - 1 - i);
	}
	AZString *key = (AZString *) azo_stack_instance_bw (&intr->stack, n_args);
	const AZImplementation *impl = azo_stack_impl_bw (&intr->stack, n_args + 1);
	if (!impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_NULL_DEREFERENCE, 1UL << AZO_EXCEPTION_NULL_DEREFERENCE, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	const AZClass *def_class;
	const AZImplementation *sub_impl;
	void *sub_inst;
	int idx = az_class_lookup_function(AZ_CLASS_FROM_IMPL(impl), impl, azo_stack_instance_bw (&intr->stack, n_args + 1), key, (AZFunctionSignature *) &sig, &def_class, &sub_impl, &sub_inst);
	if (idx >= 0) {
		AZPackedValue64 val;
		if (az_instance_get_property_by_id(def_class, AZ_CLASS_FROM_IMPL(sub_impl), sub_impl, sub_inst, idx, &val.impl, &val.v.value, 64, NULL)) {
			azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v.value);
			return ip + 2;
		}
	}
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 2;
}

static const unsigned char *
interpret_SET_PROPERTY (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	const AZClass *sub_class;
	const AZImplementation *impl, *prop_impl;
	void *inst, *prop_inst;
	AZClass *klass;
	int idx;
	unsigned int result = 0;
	// INSTANCE STRING VALUE
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 3)) return NULL;
		if (!test_stack_type_exact (intr, ip, 1, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 1);
	impl = azo_stack_impl_bw (&intr->stack, 2);
	if (!impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_NULL_DEREFERENCE, 1UL << AZO_EXCEPTION_NULL_DEREFERENCE, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	inst = azo_stack_instance_bw (&intr->stack, 2);
	if (impl == &AZBoxedInterfaceKlass.klass.impl) {
		az_boxed_interface_unbox(&impl, &inst);
	}
	klass = AZ_CLASS_FROM_IMPL(impl);
	idx = az_class_lookup_property (klass, impl, inst, key, &sub_class, &prop_impl, &prop_inst);
	if (idx >= 0) {
		AZField *prop = &sub_class->props_self[idx];
		unsigned int type = azo_stack_type_bw (&intr->stack, 0);
		if (!type && (prop->is_reference || prop->is_interface)) {
			result = az_instance_set_property_by_id (sub_class, prop_impl, prop_inst, idx, NULL, NULL, NULL);
		} else if (!az_type_is_assignable_to (type, prop->type)) {
			intr->vals[0].impl = NULL;
			intr->vals[1].impl = NULL;
			az_packed_value_set_from_impl_value (&intr->vals[0].packed_val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
			if (az_packed_value_convert (&intr->vals[1].packed_val, prop->type, &intr->vals[0].packed_val)) {
				result = az_instance_set_property_by_id (sub_class, prop_impl, prop_inst, idx, intr->vals[1].impl, az_packed_value_get_inst (&intr->vals[1].packed_val), NULL);
				az_packed_value_clear (&intr->vals[1].packed_val);
			}
			az_packed_value_clear (&intr->vals[0].packed_val);
		} else {
			result = az_instance_set_property_by_id (sub_class, prop_impl, prop_inst, idx, azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0), NULL);
		}
	}
	if (result) azo_stack_pop (&intr->stack, 3);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_BOOLEAN), &result);
	return ip + 1;
}

/* Class, String -> Value */

static const unsigned char *
interpret_GET_STATIC_PROPERTY (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	AZClass *klass;
	int idx;
	const AZClass *sub_class;
	const AZImplementation *prop_impl;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
		if (!test_stack_type_exact (intr, ip, 1, AZ_TYPE_CLASS)) return NULL;
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	klass = (AZClass *) azo_stack_instance_bw (&intr->stack, 1);

	idx = az_class_lookup_property (klass, &klass->impl, NULL, key, &sub_class, &prop_impl, NULL);
	if (idx < 0) {
		AZField *field = &sub_class->props_self[idx];
		if (field->spec == AZ_FIELD_INSTANCE) {
			/* No such field or field is instance */
		} else {
			if (az_instance_get_property_by_id (sub_class, AZ_CLASS_FROM_IMPL(prop_impl), prop_impl, NULL, idx, &intr->vals[0].impl, &intr->vals[0].v.value, 64, NULL)) {
				azo_stack_pop (&intr->stack, 2);
				azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
				return ip + 1;
			} else {
				/* Field is not readable */
			}
		}
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 1;
}

/* Class, String, Arguments -> Class, String, Arguments, Function | null */

static const unsigned char *
interpret_GET_STATIC_FUNCTION (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	AZClass *klass;
	AZFunctionSignature32 sig;
	unsigned int n_args = ip[1];
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, n_args + 2)) return NULL;
		if (!test_stack_type_exact (intr, ip, n_args + 1, AZ_TYPE_CLASS)) return NULL;
		if (!test_stack_type_exact (intr, ip, n_args, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, n_args);
	klass = (AZClass *) azo_stack_instance_bw (&intr->stack, n_args + 1);
	sig.n_args = n_args;
	sig.ret_type = AZ_TYPE_ANY;
	for (unsigned int i = 0; i < n_args; i++) {
		if (i >= 32) break;
		sig.arg_types[i] = azo_stack_type_bw (&intr->stack, n_args - 1 - i);
	}
	const AZClass *def_class;
	int idx = az_class_lookup_function(klass, NULL, NULL, key, (AZFunctionSignature *) &sig, &def_class, NULL, NULL);
	if (idx >= 0) {
		AZPackedValue64 val;
		if (az_instance_get_property_by_id(def_class, klass, NULL, NULL, idx, &val.impl, &val.v.value, 64, NULL)) {
			azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v.value);
			return ip + 2;
		}
	}
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 2;
}

static const unsigned char *
interpret_LOOKUP_PROPERTY (AZOInterpreter *intr, const unsigned char *ip)
{
	const AZImplementation *impl;
	const AZClass *sub_class;
	const AZImplementation *prop_impl;
	void *prop_inst;
	AZString *key;
	int idx;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	impl = azo_stack_impl_bw (&intr->stack, 1);
	if (!impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_NULL_DEREFERENCE, 1UL << AZO_EXCEPTION_NULL_DEREFERENCE, ( unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	idx = az_class_lookup_property (AZ_CLASS_FROM_IMPL(impl), impl, azo_stack_instance_bw (&intr->stack, 1), key, &sub_class, &prop_impl, &prop_inst);
	azo_stack_pop (&intr->stack, 1);
	if (idx >= 0) {
		AZField *prop = &sub_class->props_self[idx];
		azo_stack_push_instance (&intr->stack, prop_impl, prop_inst);
		azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_UINT32), &idx);
		azo_stack_push_instance (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_FIELD), prop);
	} else {
		azo_stack_push_value (&intr->stack, NULL, NULL);
	}
	return ip + 1;
}

static const unsigned char *
interpret_GET_ATTRIBUTE (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	void *attrd_inst;
	const AZAttribDictImplementation *attrd_impl;
	unsigned int flags;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
		if (!test_stack_type_implements (intr, ip, 1, AZ_TYPE_ATTRIBUTE_DICT)) return NULL;
		if (!test_stack_type_exact (intr, ip, 0, AZ_TYPE_STRING)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 1), azo_stack_instance_bw (&intr->stack, 1), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	intr->vals[0].impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, key, &intr->vals[0].v, &flags);
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
	return ip + 1;
}

static const unsigned char *
interpret_SET_ATTRIBUTE (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	void *attrd_inst;
	const AZAttribDictImplementation *attrd_impl;
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 3)) return NULL;
		if (!test_stack_type_implements (intr, ip, 2, AZ_TYPE_ATTRIBUTE_DICT)) return NULL;
		if (!test_stack_type_exact (intr, ip, 1, AZ_TYPE_STRING)) return NULL;
	}
	key = ( AZString *) azo_stack_instance_bw (&intr->stack, 1);
	attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 2), azo_stack_instance_bw (&intr->stack, 2), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	az_attrib_dict_set (attrd_impl, attrd_inst, key, azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0), 0);
	azo_stack_pop (&intr->stack, 3);
	return ip + 1;
}

/* End new stack methods */

void
interpreter_interpret (AZOInterpreter *intr, AZOProgram *prog, const AZImplementation **ret_impl, AZValue *ret_val, unsigned int ret_size)
{
	const unsigned char *ipc;
	AZOProgram *last_prog;

	last_prog = intr->prog;
	intr->prog = prog;

	//fprintf (stderr, "Starting interpreter\n");
	//azo_stack_print_contents (&intr->stack, stderr);

	ipc = prog->tcode;
	while (*ipc != END) {
		// double start = arikkei_get_time ();
		switch (*ipc & 127) {

		case AZO_TC_EXCEPTION:
		case AZO_TC_EXCEPTION_IF:
		case AZO_TC_EXCEPTION_IF_NOT:
			ipc = interpret_EXCEPTION (intr, ipc);
			break;

		case AZO_TC_DEBUG:
			ipc = interpret_DEBUG (intr, prog, ipc);
			break;

		/* Stack management */
		case AZO_TC_PUSH_FRAME:
			ipc = interpret_PUSH_FRAME (intr, ipc);
			break;
		case AZO_TC_POP_FRAME:
			ipc = interpret_POP_FRAME (intr, ipc);
			break;
		case AZO_TC_POP:
			ipc = interpret_POP (intr, ipc);
			break;
		case AZO_TC_REMOVE:
			ipc = interpret_REMOVE (intr, ipc);
			break;
		case AZO_TC_PUSH_EMPTY:
			ipc = interpret_PUSH_EMPTY (intr, ipc);
			break;
		case PUSH_IMMEDIATE:
			ipc = interpret_PUSH_IMMEDIATE (intr, ipc);
			break;
		case AZO_TC_PUSH_VALUE:
			ipc = interpret_PUSH_VALUE (intr, prog, ipc);
			break;
		case DUPLICATE:
			ipc = interpret_DUPLICATE (intr, ipc);
			break;
		case DUPLICATE_FRAME:
			ipc = interpret_DUPLICATE_FRAME (intr, ipc);
			break;
		case EXCHANGE:
			ipc = interpret_EXCHANGE (intr, ipc);
			break;
		case AZO_TC_EXCHANGE_FRAME:
			ipc = interpret_EXCHANGE_FRAME (intr, ipc);
			break;

		/* Type tests */
		case AZO_TC_TYPE_EQUALS:
		case AZO_TC_TYPE_IS:
		case AZO_TC_TYPE_IS_SUPER:
		case AZO_TC_TYPE_IMPLEMENTS:
			ipc = interpret_TYPE (intr, ipc);
			break;
		case AZO_TC_TYPE_EQUALS_IMMEDIATE:
		case AZO_TC_TYPE_IS_IMMEDIATE:
		case AZO_TC_TYPE_IS_SUPER_IMMEDIATE:
		case AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE:
			ipc = interpret_TYPE_IMMEDIATE (intr, ipc);
			break;
		case TYPE_OF:
		case TYPE_OF_CLASS:
			ipc = interpret_TYPE_OF (intr, ipc);
			break;

		/* Jumps */
		case JMP_32:
			ipc = interpret_JMP_32 (intr, ipc);
			break;
		case JMP_32_IF:
		case JMP_32_IF_NOT:
		case JMP_32_IF_ZERO:
		case JMP_32_IF_POSITIVE:
		case JMP_32_IF_NEGATIVE:
			ipc = interpret_JMP_32_COND (intr, ipc);
			break;

		/* Conversions */
		case POINTER_TO_U64:
			ipc = interpret_POINTER_TO_U64 (intr, ipc);
			break;
		case U64_TO_POINTER:
			ipc = interpret_POINTER_TO_U64 (intr, ipc);
			break;
		case PROMOTE:
			ipc = interpret_PROMOTE (intr, ipc);
			break;

		/* Comparisons */
		case EQUAL_TYPED:
			ipc = interpret_EQUAL_TYPED (intr, ipc);
			break;
		case EQUAL:
			ipc = interpret_EQUAL (intr, ipc);
			break;
		case COMPARE_TYPED:
			ipc = interpret_COMPARE_TYPED (intr, ipc);
			break;
		case COMPARE:
			ipc = interpret_COMPARE (intr, ipc);
			break;

		case AZO_TC_LOGICAL_NOT:
			ipc = interpret_LOGICAL_NOT (intr, ipc);
			break;
		case AZO_TC_NEGATE:
			ipc = interpret_NEGATE (intr, ipc);
			break;
		case AZO_TC_CONJUGATE:
			ipc = interpret_CONJUGATE (intr, ipc);
			break;
		case AZO_TC_BITWISE_NOT:
			ipc = interpret_BITWISE_NOT (intr, ipc);
			break;

		case AZO_TC_LOGICAL_AND:
		case AZO_TC_LOGICAL_OR:
			ipc = interpret_LOGICAL_BINARY (intr, ipc);
			break;
		case AZO_TC_ADD_TYPED:
			ipc = interpret_ADD_TYPED (intr, ipc);
			break;
		case AZO_TC_SUBTRACT_TYPED:
			ipc = interpret_SUBTRACT_TYPED (intr, ipc);
			break;
		case AZO_TC_MULTIPLY_TYPED:
			ipc = interpret_MULTIPLY_TYPED (intr, ipc);
			break;
		case AZO_TC_DIVIDE_TYPED:
			ipc = interpret_DIVIDE_TYPED (intr, ipc);
			break;
		case AZO_TC_MODULO_TYPED:
			ipc = interpret_MODULO_TYPED (intr, ipc);
			break;
		case AZO_TC_ADD:
			ipc = interpret_ADD (intr, ipc);
			break;
		case AZO_TC_SUBTRACT:
			ipc = interpret_SUBTRACT (intr, ipc);
			break;
		case AZO_TC_MULTIPLY:
			ipc = interpret_MULTIPLY (intr, ipc);
			break;
		case AZO_TC_DIVIDE:
			ipc = interpret_DIVIDE (intr, ipc);
			break;
		case AZO_TC_MODULO:
			ipc = interpret_MODULO (intr, ipc);
			break;

		case MIN_TYPED:
		case MAX_TYPED:
			ipc = interpret_MIN_MAX_TYPED (intr, ipc);
			break;

		case AZO_TC_GET_INTERFACE_IMMEDIATE:
			ipc = interpret_GET_INTERFACE_IMMEDIATE (intr, ipc);
			break;
		case AZO_TC_INVOKE:
			ipc = interpret_INVOKE (intr, ipc);
			break;
		case AZO_TC_RETURN:
			ipc = interpret_RETURN (intr, ipc, ret_impl, ret_val, ret_size);
			break;
		case AZO_TC_BIND:
			ipc = interpret_BIND (intr, ipc);
			break;

		case NEW_ARRAY:
			ipc = interpret_NEW_ARRAY (intr, ipc);
			break;
		case LOAD_ARRAY_ELEMENT:
			ipc = interpret_LOAD_ARRAY_ELEMENT (intr, ipc);
			break;
		case WRITE_ARRAY_ELEMENT:
			ipc = interpret_WRITE_ARRAY_ELEMENT (intr, ipc);
			break;

		case AZO_TC_GET_GLOBAL:
			ipc = interpret_GET_GLOBAL (intr, ipc);
			break;
		case AZO_TC_GET_PROPERTY:
			ipc = interpret_GET_PROPERTY (intr, ipc);
			break;
		case AZO_TC_GET_FUNCTION:
			ipc = interpret_GET_FUNCTION (intr, prog, ipc);
			break;
		case AZO_TC_SET_PROPERTY:
			ipc = interpret_SET_PROPERTY (intr, ipc);
			break;
		case AZO_TC_GET_STATIC_PROPERTY:
			ipc = interpret_GET_STATIC_PROPERTY (intr, ipc);
			break;
		case AZO_TC_GET_STATIC_FUNCTION:
			ipc = interpret_GET_STATIC_FUNCTION (intr, ipc);
			break;
		case AZO_TC_LOOKUP_PROPERTY:
			ipc = interpret_LOOKUP_PROPERTY (intr, ipc);
			break;
		case GET_ATTRIBUTE:
			ipc = interpret_GET_ATTRIBUTE (intr, ipc);
			break;
		case SET_ATTRIBUTE:
			ipc = interpret_SET_ATTRIBUTE (intr, ipc);
			break;

		case NOP:
			ipc += 1;
			break;

		default:
			intr->prog = last_prog;
			return;
		}
		//if ((arikkei_get_time () - start) > 0.01) {
		//	fprintf (stderr, "TC %u got %.3f\n", tc, arikkei_get_time () - start);
		//}
		if (!ipc) {
			if (intr->exc.type != AZO_EXCEPTION_NONE) {
				unsigned char b[1024];
				/* Exception */
				az_instance_to_string (&azo_exception_class->klass.impl, &intr->exc, b, 1024);
				fprintf (stderr, "Fatal exception: %s\n", b);
				azo_intepreter_print_stack (intr, stderr);
				fprintf (stderr, "\n");

				/* fixme: Clear for now (otherwise return will invoke exceptions) */
				intr->exc.type = AZO_EXCEPTION_NONE;
			}
			break;
		}
		if ((ipc - prog->tcode) >= prog->tcode_length) break;
	}
	intr->prog = last_prog;
}

void
azo_intepreter_print_stack (AZOInterpreter *intr, FILE *ofs)
{
	unsigned int i;
	for (i = 0; i < intr->n_frames; i++) {
		unsigned int frame = intr->n_frames - 1 - i;
		unsigned int end = (frame == (intr->n_frames - 1)) ? intr->stack.length : intr->frames[frame + 1];
		if (end > intr->frames[frame]) {
			fprintf (stderr, "Frame %u (%u - %u)\n", frame, intr->frames[frame], end - 1);
		} else {
			fprintf (stderr, "Frame %u EMPTY\n", frame);
		}
		azo_stack_print_contents (&intr->stack, intr->frames[frame], end, stderr);
	}
}
