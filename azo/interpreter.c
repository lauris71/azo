#define __AZO_INTERPRETER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define DEBUG_COMP 1

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <arikkei/arikkei-iolib.h>
#include <arikkei/arikkei-strlib.h>

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

	intr->flags = AZO_INTR_FLAG_CHECK_ARGS;

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

unsigned int
azo_interpreter_push_frame (AZOInterpreter *intr, unsigned int n_elements)
{
	if (intr->n_frames >= intr->size_frames) {
		intr->size_frames = intr->size_frames << 1;
		intr->frames = ( unsigned int *) realloc (intr->frames, intr->size_frames * 4);
	}
	intr->frames[intr->n_frames] = intr->stack.length - n_elements;
	return intr->n_frames++;
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

#define EXCEPTION(type) azo_interpreter_exception(intr, ip, type);
#define EXCEPTION_THROW(type) return azo_interpreter_exception(intr, ip, type), NULL;

/* Convert value to u32, return 1 on success, fill exception data and return 0 on failure */

static unsigned int
convert_to_u32 (AZOInterpreter *intr, unsigned int *dst, const AZImplementation *impl, void *val, const uint8_t *ip)
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
		EXCEPTION(AZO_EXCEPTION_INVALID_CONVERSION);
		return 0;
	}
	if (negative || overflow) {
		EXCEPTION(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE);
	} else {
		EXCEPTION(AZO_EXCEPTION_CHANGE_OF_PRECISION);
	}
	return 0;
}

/* New stack methods */

static unsigned int
test_stack_type_exact (AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if ((pos + 1) > intr->stack.length) {
		EXCEPTION(AZO_EXCEPTION_STACK_UNDERFLOW);
		return 0;
	}
	if (azo_stack_type_bw (&intr->stack, pos) != type) {
		EXCEPTION(AZO_EXCEPTION_INVALID_TYPE);
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_is (AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if ((pos + 1) > intr->stack.length) {
		EXCEPTION(AZO_EXCEPTION_STACK_UNDERFLOW);
		return 0;
	}
	if (!az_type_is_a (azo_stack_type_bw (&intr->stack, pos), type)) {
		EXCEPTION(AZO_EXCEPTION_INVALID_TYPE);
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

#define TEST(cond, type) if (!(cond)) EXCEPTION_THROW(type);
#define CHECK(cond, type) if (((intr->flags & AZO_INTR_FLAG_CHECK_ARGS) || (ip[0] & AZO_TC_CHECK_ARGS)) && !(cond)) EXCEPTION_THROW(type);
#define CHECK_UNDERFLOW(n_values) CHECK(n_values <= intr->stack.length, AZO_EXCEPTION_STACK_UNDERFLOW);
#define TEST_OVERFLOW(n_values) TEST((intr->stack.length + n_values) <= 65536, AZO_EXCEPTION_STACK_OVERFLOW);

#define CHECK_TYPE_EXACT(pos,type) if ((intr->flags & AZO_INTR_FLAG_CHECK_ARGS) || (ip[0] & AZO_TC_CHECK_ARGS)) if(!test_stack_type_exact(intr, ip, pos, type)) return NULL;

static const unsigned char *
interpret_EXCEPTION (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if ((*ip & 0x7f) == AZO_TC_EXCEPTION_IF) {
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if(!azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	} else if ((*ip & 0x7f) == AZO_TC_EXCEPTION_IF_NOT) {
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if (azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	}
	azo_exception_set (&intr->exc, type, 1U << type, (unsigned int) (ip - intr->prog->tcode));
	return NULL;
}

/*
* AZO_TC_EXCEPTION_IF_TYPE_IS_NOT pos type
* Throw INVALID_TYPE if element at pos is not type
*/
static const unsigned char *
interpret_type_EXCEPTION (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos, type;
	memcpy (&pos, ip + 1, 4);
	memcpy (&type, ip + 5, 4);
	if ((*ip & AZO_TC_CHECK_ARGS) && !test_stack_underflow (intr, ip, pos + 1)) return NULL;
	if (az_type_is_a(azo_stack_type_bw(&intr->stack, pos), type)) {
		return ip + 9;
	}
	azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1U << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
	return NULL;
}

static const unsigned char *
interpret_DEBUG (AZOInterpreter *intr, AZOProgram *prog, const unsigned char *ip)
{
	uint32_t op;
	memcpy (&op, ip + 1, 4);
	if ((*ip & 127) == AZO_TC_DEBUG) {
		fprintf (stderr, "Debug: IPC = %u\n", (unsigned int) (ip - intr->prog->tcode));
		azo_intepreter_print_stack (intr, stderr);
	} else {
		fprintf (stderr, "Debug: %s\n", prog->values[op].v.string->str);
	}
	return ip + 5;
}

static const unsigned char *
interpret_PUSH_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t start;
	memcpy (&start, ip + 1, 4);
	CHECK_UNDERFLOW(start + 1);
	azo_interpreter_push_frame (intr, start);
	return ip + 5;
}

static const unsigned char *
interpret_POP_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK(intr->n_frames > 0, AZO_EXCEPTION_STACK_UNDERFLOW);
	CHECK(intr->frames[intr->n_frames - 1] <= intr->stack.length, AZO_EXCEPTION_STACK_UNDERFLOW);
	azo_interpreter_pop_frame (intr);
	return ip + 1;
}

static const unsigned char *
interpret_POP (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int n_values;
	memcpy (&n_values, ip + 1, 4);
	CHECK_UNDERFLOW(n_values);
	azo_stack_pop (&intr->stack, n_values);
	return ip + 5;
}

static const unsigned char *
interpret_REMOVE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int first, n_values;
	memcpy (&first, ip + 1, 4);
	memcpy (&n_values, ip + 5, 4);
	CHECK_UNDERFLOW(first + n_values);
	azo_stack_remove (&intr->stack, intr->stack.length - (first + n_values), n_values);
	return ip + 9;
}

static const unsigned char *
interpret_PUSH_EMPTY (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	TEST_OVERFLOW(1);
	if (type) {
		AZClass *klass = az_type_get_class (type);
		CHECK(!AZ_CLASS_IS_ABSTRACT(klass), AZO_EXCEPTION_INVALID_TYPE);
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
	TEST_OVERFLOW(1);
	/* Type has to be primitive or NULL */
	CHECK(!type || AZ_TYPE_IS_PRIMITIVE(type), AZO_EXCEPTION_INVALID_TYPE);
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
	TEST_OVERFLOW(1);
	azo_stack_push_value (&intr->stack, prog->values[loc].impl, &prog->values[loc].v);
	return ip + 5;
}

static const unsigned char *
interpret_DUPLICATE (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	CHECK_UNDERFLOW(pos + 1);
	TEST_OVERFLOW(1);
	azo_stack_duplicate (&intr->stack, intr->stack.length - 1 - pos);
	return ip + 5;
}

static const unsigned char *
interpret_DUPLICATE_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	CHECK((intr->frames[intr->n_frames - 1] + pos) < intr->stack.length, AZO_EXCEPTION_STACK_UNDERFLOW);
	TEST_OVERFLOW(1);
	azo_stack_duplicate (&intr->stack, intr->frames[intr->n_frames - 1] + pos);
	return ip + 5;
}

static const unsigned char *
interpret_EXCHANGE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos;
	memcpy (&pos, ip + 1, 4);
	CHECK_UNDERFLOW(pos + 1);
	azo_stack_exchange (&intr->stack, intr->stack.length - 1 - pos);
	return ip + 5;
}

static const unsigned char *
interpret_EXCHANGE_FRAME (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos;
	memcpy (&pos, ip + 1, 4);
	CHECK((intr->frames[intr->n_frames - 1] + pos) < intr->stack.length, AZO_EXCEPTION_STACK_UNDERFLOW);
	azo_stack_exchange (&intr->stack, intr->frames[intr->n_frames - 1] + pos);
	return ip + 5;
}

static const unsigned char *
interpret_TYPE (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int pos, result;
	pos = ip[1];
	CHECK_TYPE_EXACT(0, AZ_TYPE_UINT32);
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
		CHECK_TYPE_EXACT(0, AZ_TYPE_CLASS);
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
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if (azo_stack_boolean_bw(&intr->stack, 0)) dst += raddr;
		break;
	case JMP_32_IF_NOT:
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if (!azo_stack_boolean_bw(&intr->stack, 0)) dst += raddr;
		break;
	case JMP_32_IF_ZERO:
		CHECK_TYPE_EXACT(0, AZ_TYPE_INT32);
		if (azo_stack_int32_bw(&intr->stack, 0) == 0) dst += raddr;
		break;
	case JMP_32_IF_POSITIVE:
		CHECK_TYPE_EXACT(0, AZ_TYPE_INT32);
		if (azo_stack_int32_bw(&intr->stack, 0) > 0) dst += raddr;
		break;
	case JMP_32_IF_NEGATIVE:
		CHECK_TYPE_EXACT(0, AZ_TYPE_INT32);
		if (azo_stack_int32_bw(&intr->stack, 0) < 0) dst += raddr;
		break;
	}
	azo_stack_pop (&intr->stack, 1);
	return dst;
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
	CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
	val = azo_stack_value_bw (&intr->stack, 0);
	val->boolean_v = !val->boolean_v;
	return ip + 1;
}

static const unsigned char *
interpret_NEGATE (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	unsigned int type = azo_stack_type_bw(&intr->stack, 0);
	CHECK(AZ_TYPE_IS_SIGNED(type) || (type == AZ_TYPE_COMPLEX_FLOAT) || (type == AZ_TYPE_COMPLEX_DOUBLE), AZO_EXCEPTION_INVALID_TYPE);
	AZValue *val = azo_stack_value_bw(&intr->stack, 0);
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
	CHECK_UNDERFLOW(1);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK((type == AZ_TYPE_COMPLEX_FLOAT) || (type == AZ_TYPE_COMPLEX_DOUBLE), AZO_EXCEPTION_INVALID_TYPE);
	AZValue *val = azo_stack_value_bw (&intr->stack, 0);
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
	CHECK_UNDERFLOW(1);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK(AZ_TYPE_IS_INTEGRAL(type), AZO_EXCEPTION_INVALID_TYPE);
	AZValue *val = azo_stack_value_bw (&intr->stack, 0);
	val->uint64_v = ~val->int64_v;
	return ip + 1;
}

static const unsigned char *
interpret_LOGICAL_BINARY (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue *lhs, *rhs;
	CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
	CHECK_TYPE_EXACT(1, AZ_TYPE_BOOLEAN);
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

static const uint8_t *
add (AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	switch(type) {
	case AZ_TYPE_INT32:
		lhs->int32_v += rhs->int32_v;
		break;
	case AZ_TYPE_UINT32:
		lhs->uint32_v += rhs->uint32_v;
		break;
	case AZ_TYPE_INT64:
		lhs->int64_v += rhs->int64_v;
		break;
	case AZ_TYPE_UINT64:
		lhs->uint64_v += rhs->uint64_v;
		break;
	case AZ_TYPE_FLOAT:
		lhs->float_v += rhs->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		lhs->double_v += rhs->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		lhs->cfloat_v.r += rhs->cfloat_v.r;
		lhs->cfloat_v.i += rhs->cfloat_v.i;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		lhs->cdouble_v.r += rhs->cdouble_v.r;
		lhs->cdouble_v.i += rhs->cdouble_v.i;
		break;
	default:
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_ADD_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type = ip[1];
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	ip = add(intr, ip, type);
	return (ip) ? ip + 1 : NULL;
}

static const unsigned char *
interpret_ADD (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(2);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK_TYPE_EXACT(1, type);
	return add(intr, ip, type);
}

static const unsigned char *
subtract (AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	switch(type) {
	case AZ_TYPE_INT32:
		lhs->int32_v -= rhs->int32_v;
		break;
	case AZ_TYPE_UINT32:
		lhs->uint32_v -= rhs->uint32_v;
		break;
	case AZ_TYPE_INT64:
		lhs->int64_v -= rhs->int64_v;
		break;
	case AZ_TYPE_UINT64:
		lhs->uint64_v -= rhs->uint64_v;
		break;
	case AZ_TYPE_FLOAT:
		lhs->float_v -= rhs->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		lhs->double_v -= rhs->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		lhs->cfloat_v.r -= rhs->cfloat_v.r;
		lhs->cfloat_v.i -= rhs->cfloat_v.i;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		lhs->cdouble_v.r -= rhs->cdouble_v.r;
		lhs->cdouble_v.i -= rhs->cdouble_v.i;
		break;
	default:
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_SUBTRACT_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type = ip[1];
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	ip = subtract(intr, ip, type);
	return (ip) ? ip + 1 : NULL;
}

static const uint8_t *
interpret_SUBTRACT (AZOInterpreter *intr, const uint8_t *ip)
{
	CHECK_UNDERFLOW(2);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK_TYPE_EXACT(1, type);
	return subtract(intr, ip, type);
}

static const uint8_t *
multiply (AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	AZValue *lhs = azo_stack_value_bw(&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw(&intr->stack, 0);
	switch(type) {
	case AZ_TYPE_INT32:
		lhs->int32_v *= rhs->int32_v;
		break;
	case AZ_TYPE_UINT32:
		lhs->uint32_v *= rhs->uint32_v;
		break;
	case AZ_TYPE_INT64:
		lhs->int64_v *= rhs->int64_v;
		break;
	case AZ_TYPE_UINT64:
		lhs->uint64_v *= rhs->uint64_v;
		break;
	case AZ_TYPE_FLOAT:
		lhs->float_v *= rhs->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		lhs->double_v *= rhs->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT:
		lhs->cfloat_v.r = lhs->cfloat_v.r * rhs->cfloat_v.r - lhs->cfloat_v.i * rhs->cfloat_v.i;
		lhs->cfloat_v.i = lhs->cfloat_v.r * rhs->cfloat_v.i + lhs->cfloat_v.i * rhs->cfloat_v.r;
		break;
	case AZ_TYPE_COMPLEX_DOUBLE:
		lhs->cdouble_v.r = lhs->cdouble_v.r * rhs->cdouble_v.r - lhs->cdouble_v.i * rhs->cdouble_v.i;
		lhs->cdouble_v.i = lhs->cdouble_v.r * rhs->cdouble_v.i + lhs->cdouble_v.i * rhs->cdouble_v.r;
		break;
	default:
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_MULTIPLY_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type = ip[1];
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	ip = multiply(intr, ip, type);
	return (ip) ? ip + 1 : NULL;
}

static const unsigned char *
interpret_MULTIPLY (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(2);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK_TYPE_EXACT(1, type);
	return multiply(intr, ip, type);
}

static const uint8_t *
divide (AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	switch(type) {
	case AZ_TYPE_INT32:
		lhs->int32_v /= rhs->int32_v;
		break;
	case AZ_TYPE_UINT32:
		lhs->uint32_v /= rhs->uint32_v;
		break;
	case AZ_TYPE_INT64:
		lhs->int64_v /= rhs->int64_v;
		break;
	case AZ_TYPE_UINT64:
		lhs->uint64_v /= rhs->uint64_v;
		break;
	case AZ_TYPE_FLOAT:
		lhs->float_v /= rhs->float_v;
		break;
	case AZ_TYPE_DOUBLE:
		lhs->double_v /= rhs->double_v;
		break;
	case AZ_TYPE_COMPLEX_FLOAT: {
		float a = lhs->cfloat_v.r;
		float b = lhs->cfloat_v.i;
		float c = rhs->cfloat_v.r;
		float d = rhs->cfloat_v.i;
		lhs->cfloat_v.r = (a * c + b * d) / (c * c + d * d);
		lhs->cfloat_v.i = (b * c - a * d) / (c * c + d * d);
		break;
	}
	case AZ_TYPE_COMPLEX_DOUBLE: {
		double a = lhs->cdouble_v.r;
		double b = lhs->cdouble_v.i;
		double c = rhs->cdouble_v.r;
		double d = rhs->cdouble_v.i;
		lhs->cdouble_v.r = (a * c + b * d) / (c * c + d * d);
		lhs->cdouble_v.i = (b * c - a * d) / (c * c + d * d);
		break;
	}
	default:
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_DIVIDE_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type = ip[1];
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	ip = divide(intr, ip, type);
	return (ip) ? ip + 1 : NULL;
}

static const uint8_t *
interpret_DIVIDE (AZOInterpreter *intr, const uint8_t *ip)
{
	CHECK_UNDERFLOW(2);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK_TYPE_EXACT(1, type);
	return divide(intr, ip, type);
}

static const uint8_t *
modulo (AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	switch(type) {
	case AZ_TYPE_INT32:
		lhs->int32_v %= rhs->int32_v;
		break;
	case AZ_TYPE_UINT32:
		lhs->uint32_v %= rhs->uint32_v;
		break;
	case AZ_TYPE_INT64:
		lhs->int64_v %= rhs->int64_v;
		break;
	case AZ_TYPE_UINT64:
		lhs->uint64_v %= rhs->uint64_v;
		break;
	case AZ_TYPE_FLOAT:
		lhs->float_v = fmodf(lhs->float_v, rhs->float_v);
		break;
	case AZ_TYPE_DOUBLE:
		lhs->double_v = fmod(lhs->double_v, rhs->double_v);
		break;
	default:
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_MODULO_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type = ip[1];
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	ip = modulo(intr, ip, type);
	return (ip) ? ip + 1 : NULL;
}

static const uint8_t *
interpret_MODULO (AZOInterpreter *intr, const uint8_t *ip)
{
	CHECK_UNDERFLOW(2);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK_TYPE_EXACT(1, type);
	return modulo(intr, ip, type);
}

static const unsigned char *
interpret_MIN_MAX_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int type;
	unsigned int remove;
	type = ip[1];
	if (!test_stack_type_exact_2 (intr, ip, type)) return NULL;
	AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	if (type == AZ_TYPE_INT8) {
		remove = lhs->int8_v > rhs->int8_v;
	} else if (type == AZ_TYPE_UINT8) {
		remove = lhs->uint8_v > rhs->uint8_v;
	} else if (type == AZ_TYPE_INT16) {
		remove = lhs->int16_v > rhs->int16_v;
	} else if (type == AZ_TYPE_UINT16) {
		remove = lhs->uint16_v > rhs->uint16_v;
	} else if (type == AZ_TYPE_INT32) {
		remove = lhs->int32_v > rhs->int32_v;
	} else if (type == AZ_TYPE_UINT32) {
		remove = lhs->uint32_v > rhs->uint32_v;
	} else if (type == AZ_TYPE_INT64) {
		remove = lhs->int64_v > rhs->int64_v;
	} else if (type == AZ_TYPE_UINT64) {
		remove = lhs->uint64_v > rhs->uint64_v;
	} else if (type == AZ_TYPE_FLOAT) {
		remove = lhs->float_v > rhs->float_v;
	} else if (type == AZ_TYPE_DOUBLE) {
		remove = lhs->double_v > rhs->double_v;
	} else {
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	if (ip[0] == MAX_TYPED) remove = !remove;
	azo_stack_remove (&intr->stack, intr->stack.length - 1 - remove, 1);
	return ip + 2;
}

static const unsigned char *
interpret_GET_INTERFACE_IMMEDIATE (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!AZ_IMPL_IS_BLOCK(azo_stack_impl_bw(&intr->stack, 0))) {
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
		}
		if (!AZ_TYPE_IS_INTERFACE(type)) {
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
		}
	}
	void *inst;
	const AZImplementation *impl = az_instance_get_interface(azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0), type, &inst);
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
	CHECK_UNDERFLOW(1);
	AZFunctionInstance *func_inst;
	const AZFunctionImplementation *func_impl = (const AZFunctionImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, pos), azo_stack_instance_bw (&intr->stack, pos), AZ_TYPE_FUNCTION, (void **) &func_inst);
	if (!func_impl) {
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
	}
	const AZFunctionSignature *sig = az_function_get_signature (func_impl, func_inst);
	for (unsigned int i = 0; i < sig->n_args; i++) {
		if (!azo_stack_convert_bw (&intr->stack, sig->n_args - 1 - i, sig->arg_types[i])) {
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_TYPE);
		}
	}
	intr->vals[0].impl = NULL;
	az_function_invoke (func_impl, func_inst, azo_stack_impls_bw (&intr->stack, sig->n_args - 1), (const AZValue **) azo_stack_values_bw (&intr->stack, sig->n_args - 1), &intr->vals[0].impl, &intr->vals[0].v, NULL);
	azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
	intr->vals[0].impl = NULL;
	return ip + 2;
}

static const unsigned char *
interpret_RETURN (AZOInterpreter *intr, const unsigned char *ip)
{
	return NULL;
}

static const unsigned char *
interpret_RETURN_VALUE (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	intr->vals[0].impl = az_value_copy_autobox(azo_stack_impl_bw(&intr->stack, 0), &intr->vals[0].v.value, azo_stack_value_bw(&intr->stack, 0), 64);
	return NULL;
}

static const unsigned char *
interpret_BIND (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t pos;
	memcpy (&pos, ip + 1, 4);
	CHECK_TYPE_EXACT(pos, AZO_TYPE_COMPILED_FUNCTION);
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) azo_stack_instance_bw (&intr->stack, pos);
	for (unsigned int i = 0; i < pos; i++) {
		azo_compiled_function_bind (cfunc, i, azo_stack_impl_bw (&intr->stack, pos - 1 - i), azo_stack_instance_bw (&intr->stack, pos - 1 - i));
	}
	if (pos) azo_stack_pop (&intr->stack, pos);
	return ip + 5;
}

static const unsigned char *
interpret_NEW_ARRAY (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	unsigned int size;
	AZValueArrayRef *varray;
	AZClass *varray_class;
	if (*ip & AZO_TC_CHECK_ARGS) {
		if (!convert_to_u32 (intr, &size, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0), ip)) {
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
		if (!convert_to_u32 (intr, &idx, idx_impl, azo_stack_value_bw (&intr->stack, 0), ip)) {
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
		if (!convert_to_u32 (intr, &idx, azo_stack_impl_bw (&intr->stack, 1), azo_stack_value_bw (&intr->stack, 1), ip)) {
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
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
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
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
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
	CHECK_TYPE_EXACT(n_args, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, n_args + 2)) return NULL;
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
	CHECK_TYPE_EXACT(1, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 3)) return NULL;
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
	CHECK_TYPE_EXACT(1, AZ_TYPE_CLASS);
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
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
	CHECK_TYPE_EXACT(n_args + 1, AZ_TYPE_CLASS);
	CHECK_TYPE_EXACT(n_args, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, n_args + 2)) return NULL;
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
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
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
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 2)) return NULL;
		if (!test_stack_type_implements (intr, ip, 1, AZ_TYPE_ATTRIBUTE_DICT)) return NULL;
	}
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 1), azo_stack_instance_bw (&intr->stack, 1), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	intr->vals[0].impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, key, &intr->vals[0].v, &flags);
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v);
	return ip + 1;
}

/**
 * @brief Set value in dictionary
 * 
 * SET_ATTRIBUTE
 * [instance, key, value]
 * []
 * 
 * Throws INVALID_TYPE if instance is not dictionary
 * Throws INVALID_VALUE if attribute cannot be set
 */
static const unsigned char *
interpret_SET_ATTRIBUTE (AZOInterpreter *intr, const unsigned char *ip)
{
	void *attrd_inst;
	const AZAttribDictImplementation *attrd_impl;
	CHECK_TYPE_EXACT(1, AZ_TYPE_STRING);
	if (ip[0] & AZO_TC_CHECK_ARGS) {
		if (!test_stack_underflow (intr, ip, 3)) return NULL;
		if (!test_stack_type_implements (intr, ip, 2, AZ_TYPE_ATTRIBUTE_DICT)) return NULL;
	}
	AZString *key = (AZString *) azo_stack_instance_bw (&intr->stack, 1);
	attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 2), azo_stack_instance_bw (&intr->stack, 2), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	if (!attrd_impl) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_TYPE, 1UL << AZO_EXCEPTION_INVALID_TYPE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	if (!az_attrib_dict_set (attrd_impl, attrd_inst, key, azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0), 0)) {
		azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_VALUE, 1UL << AZO_EXCEPTION_INVALID_VALUE, (unsigned int) (ip - intr->prog->tcode));
		return NULL;
	}
	azo_stack_pop (&intr->stack, 3);
	return ip + 1;
}

/* End new stack methods */

void
azo_interpreter_exception(AZOInterpreter *intr, const uint8_t *ip, unsigned int type)
{
	azo_exception_set (&intr->exc, type, 1UL << type, (unsigned int) (ip - intr->prog->tcode));
}

const uint8_t *
azo_interpreter_interpret_tc (AZOInterpreter *intr, AZOProgram *prog, const uint8_t *ipc)
{
	switch (*ipc & 127) {
		case AZO_TC_EXCEPTION:
		case AZO_TC_EXCEPTION_IF:
		case AZO_TC_EXCEPTION_IF_NOT:
			ipc = interpret_EXCEPTION (intr, ipc);
			break;
		case AZO_TC_EXCEPTION_IF_TYPE_IS_NOT:
			ipc = interpret_type_EXCEPTION (intr, ipc);
			break;

		case AZO_TC_DEBUG:
		case AZO_TC_DEBUG_STR:
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
		case AZO_TC_DUPLICATE:
			ipc = interpret_DUPLICATE (intr, ipc);
			break;
		case AZO_TC_DUPLICATE_FRAME:
			ipc = interpret_DUPLICATE_FRAME (intr, ipc);
			break;
		case AZO_TC_EXCHANGE:
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
		case AZO_TC_TYPE_OF_CLASS:
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
			ipc = interpret_RETURN (intr, ipc);
			break;
		case AZO_TC_RETURN_VALUE:
			ipc = interpret_RETURN_VALUE (intr, ipc);
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
		case AZO_TC_SET_ATTRIBUTE:
			ipc = interpret_SET_ATTRIBUTE (intr, ipc);
			break;

		case NOP:
			ipc += 1;
			break;

		default:
			return NULL;
	}
	return ipc;
}

void
azo_interpreter_run(AZOInterpreter *intr, AZOProgram *prog)
{
	AZOProgram *last_prog = intr->prog;
	intr->prog = prog;

	const uint8_t *ipc = prog->tcode;
	const uint8_t *end = prog->tcode + prog->tcode_length;

	while (ipc && (ipc < end)) {
		ipc = azo_interpreter_interpret_tc(intr, prog, ipc);
	}

	if (intr->exc.type != AZO_EXCEPTION_NONE) {
		unsigned char b[1024];
		/* Exception */
		az_instance_to_string (&azo_exception_class->klass.impl, &intr->exc, b, 1024);
		fprintf (stderr, "Fatal exception: %s\n", b);
		fprintf(stderr, "Position: %d %d\n", intr->exc.ipc, prog->tcode[intr->exc.ipc] & 0x7f);
		azo_intepreter_print_stack (intr, stderr);
		fprintf (stderr, "\n");

		/* fixme: Clear for now (otherwise return will invoke exceptions) */
		intr->exc.type = AZO_EXCEPTION_NONE;
	}
	intr->prog = last_prog;
}

static const uint8_t *
print_stack_element_bw(AZOInterpreter *intr, unsigned int pos, uint8_t *buf, unsigned int buf_len)
{
	if ((pos + 1) > intr->stack.length) {
		arikkei_strncpy(buf, buf_len, (const uint8_t *) "UNDERFLOW");
	} else {
		const AZImplementation *impl = azo_stack_impl_bw(&intr->stack, pos);
		if (!impl) {
			arikkei_strncpy(buf, buf_len, (const uint8_t *) "VOID");
		} else {
			void *inst = azo_stack_instance_bw(&intr->stack, pos);
			unsigned int p = az_instance_to_string(impl, inst, buf, buf_len);
			p += arikkei_strncpy(buf, buf_len - p, (const uint8_t *) " ()");
			p += arikkei_strncpy(buf, buf_len - p, AZ_CLASS_FROM_IMPL(impl)->name);
			p += arikkei_strncpy(buf, buf_len - p, (const uint8_t *) ")");
		}
	}
	return buf;
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
