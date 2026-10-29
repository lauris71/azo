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
#include <az/classes/value-array.h>

#include <azo/bytecode.h>
#include <azo/calculate.h>
#include <azo/compiled-function.h>
#include <azo/private.h>
#include <azo/defer.h>

#include <azo/interpreter.h>

AZOInterpreter *
azo_interpreter_new (AZOContext *ctx)
{
	AZOInterpreter *intr = (AZOInterpreter *) malloc (sizeof (AZOInterpreter));
	memset (intr, 0, sizeof (AZOInterpreter));

	intr->size_frames = 16;
	intr->frames = (unsigned int *) malloc (intr->size_frames * 4);
	intr->frames[0] = 0;
	intr->n_frames = 1;
	intr->ctx = ctx;

	az_instance_init_by_type (&intr->stack, AZO_TYPE_STACK);

	intr->flags = AZO_INTR_FLAG_CHECK_ARGS | AZO_INTR_DEFAULT_MASK;

	return intr;
}

void
interpreter_delete (AZOInterpreter *intr)
{
	az_instance_finalize_by_type (&intr->stack, AZO_TYPE_STACK);
	free (intr->frames);
	free (intr);
}

void
azo_interpreter_init(AZOInterpreter *intr)
{
	intr->flags = AZO_INTR_FLAG_CHECK_ARGS | AZO_INTR_DEFAULT_MASK;
	intr->vals[0].impl = NULL;
	intr->vals[1].impl = NULL;
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

static void azo_interpreter_run_code(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const uint8_t *ipc, const uint8_t *end);

static void
intr_trace_push (AZOInterpreter *intr, AZOInterpreterCtx *ictx, unsigned int ip)
{
	AZOContext *ctx = intr->ctx;
	if (ctx->n_traces < 64) {
		azo_stack_trace_element_set(&ctx->traces[ctx->n_traces], ictx->prog, ip);
		ctx->n_traces += 1;
	} else {
		ctx->n_truncated += 1;
	}
}

/*
 * Unwind the current frame after an exception: pop values, run defers, scan
 * catch handle runs. On a match the run and the flags word are popped, the
 * flags restored, a fresh exception value materialized at the flags position
 * and the catch address returned; otherwise NULL (unwind continues in the
 * caller frame).
 */
static const uint8_t *
intr_unwind (AZOInterpreter *intr, AZOInterpreterCtx *ictx)
{
	unsigned int frame_base = intr->frames[intr->n_frames - 1];
	while (intr->stack.length > frame_base) {
		const AZImplementation *impl = azo_stack_impl_bw (&intr->stack, 0);
		if (impl == &azo_defer_handle_class->impl) {
			AZODeferHandle handle = *((AZODeferHandle *) azo_stack_instance_bw (&intr->stack, 0));
			azo_stack_pop (&intr->stack, 1);
			uint32_t flags = intr->flags;
			intr->flags |= AZO_INTR_FLAG_NO_THROW;
			unsigned int len = intr->stack.length;
			azo_interpreter_run_code (intr, ictx, ictx->tcode + handle.ip, ictx->tcode + handle.ip + handle.len);
			intr->flags = flags;
			/* A defer body that exits on a pending exception leaves its locals behind */
			while (intr->stack.length > len) azo_stack_pop (&intr->stack, 1);
		} else if (impl == AZ_IMPL_FROM_TYPE (azo_catch_handle_get_type ())) {
			/* Scan the contiguous handle run (top-down = declaration order) */
			unsigned int n_handles = 0;
			while (((int) intr->stack.length - 1 - (int) n_handles >= (int) frame_base) &&
				(azo_stack_impl_bw (&intr->stack, n_handles) == AZ_IMPL_FROM_TYPE (azo_catch_handle_get_type ()))) {
				n_handles++;
			}
			unsigned int catch_ip = 0;
			unsigned int found = 0;
			for (unsigned int i = 0; i < n_handles; i++) {
				const AZOCatchHandle *handle = (const AZOCatchHandle *) azo_stack_instance_bw (&intr->stack, i);
				if ((handle->type && (intr->ctx->az_ctx.exc.type == handle->type)) || (intr->ctx->az_ctx.exc.mask & handle->mask)) {
					catch_ip = handle->ip;
					found = 1;
					break;
				}
			}
			/* The flags word sits directly below the run */
			uint32_t saved_flags = azo_stack_value_bw (&intr->stack, n_handles)->uint32_v;
			azo_stack_pop (&intr->stack, n_handles + 1);
			intr->flags = saved_flags;
			if (found) {
				AZOException *exc = azo_exception_new (intr->ctx);
				AZValue val;
				val.reference = (AZReference *) exc;
				azo_stack_push_value_transfer (&intr->stack, AZ_IMPL_FROM_TYPE (azo_exception_get_type ()), &val);
				return ictx->tcode + catch_ip;
			}
		} else {
			azo_stack_pop (&intr->stack, 1);
		}
	}
	return NULL;
}

static void
azo_interpreter_run_code(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const uint8_t *ipc, const uint8_t *end)
{
	while (ipc && (ipc < end)) {
		unsigned int ip = ipc - ictx->tcode;
		assert((ictx->tcode <= ipc) && (ipc < end));
		ipc = azo_interpreter_interpret_tc(intr, ictx, ipc);
		if (!ipc && intr->ctx->az_ctx.exc.type && !(intr->flags & AZO_INTR_FLAG_NO_THROW)) {
			/* The exception was raised at ip (throw or call) - record it and unwind the frame;
			 * native code may raise without setting an ip - then the call site is the best we have */
			if (!intr->ctx->exc_ip) intr->ctx->exc_ip = ictx->tcode + ip;
			intr_trace_push (intr, ictx, ip);
			ipc = intr_unwind (intr, ictx);
		}
	}
}

static void
intr_rewind(AZOInterpreter *intr, AZOInterpreterCtx *ictx, unsigned int n_values)
{
	uint32_t flags = intr->flags;
	intr->flags |= AZO_INTR_FLAG_NO_THROW;
	for (unsigned int i = 0; i < n_values; i++) {
		if (azo_stack_impl_bw(&intr->stack, 0) == &azo_defer_handle_class->impl) {
			AZODeferHandle handle = *((AZODeferHandle *) azo_stack_instance_bw(&intr->stack, 0));
			azo_stack_pop(&intr->stack, 1);
			unsigned int len = intr->stack.length;
			azo_interpreter_run_code(intr, ictx, ictx->tcode + handle.ip, ictx->tcode + handle.ip + handle.len);
			/* A defer body that exits on a pending exception leaves its locals behind */
			while (intr->stack.length > len) azo_stack_pop (&intr->stack, 1);
		} else {
			azo_stack_pop(&intr->stack, 1);
		}
	}
	intr->flags = flags;
}

unsigned int
azo_interpreter_push_frame (AZOInterpreter *intr, unsigned int n_elements)
{
	if (intr->n_frames >= intr->size_frames) {
		intr->size_frames = intr->size_frames << 1;
		intr->frames = (unsigned int *) realloc (intr->frames, intr->size_frames * 4);
	}
	intr->frames[intr->n_frames] = intr->stack.length - n_elements;
	return intr->n_frames++;
}

unsigned int
azo_interpreter_pop_frame (AZOInterpreter *intr, AZOInterpreterCtx *ictx)
{
	intr_rewind(intr, ictx, intr->stack.length - intr->frames[intr->n_frames - 1]);
	intr->n_frames -= 1;
	return intr->n_frames;
}

static inline void
set_exception_if_clear(AZOInterpreter *intr, unsigned int type, const uint8_t *ip) {
	if (!intr->ctx->az_ctx.exc.type) {
		/* The category mask is looked up from the context exception registry */
		azo_context_set_exception(intr->ctx, type, NULL, ip);
	}
}

#define EXCEPTION(type) set_exception_if_clear(intr, type, ip);
#define EXCEPTION_THROW(type) { set_exception_if_clear(intr, type, ip); if (!(intr->flags & AZO_INTR_FLAG_NO_THROW)) return NULL; }

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
test_stack_type_exact(AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if (azo_stack_type_bw (&intr->stack, pos) != type) {
		EXCEPTION(AZ_EXCEPTION_INVALID_TYPE);
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_type_is(AZOInterpreter *intr, const unsigned char *ip, unsigned int pos, unsigned int type)
{
	if (!az_type_is_a(azo_stack_type_bw(&intr->stack, pos), type)) {
		EXCEPTION(AZ_EXCEPTION_INVALID_TYPE);
		return 0;
	}
	return 1;
}

static unsigned int
test_stack_overflow (AZOInterpreter *intr, const uint8_t *ip, unsigned int n_values)
{
	if ((intr->stack.length + n_values) > 65536) {
		azo_context_set_exception(intr->ctx, AZO_EXCEPTION_STACK_OVERFLOW, NULL, ip);
		return 0;
	}
	return 1;
}

#define TEST(cond, type) if (!(cond)) EXCEPTION_THROW(type);
#define CHECK(cond, type) if ((intr->flags & AZO_INTR_FLAG_CHECK_ARGS) && !(cond)) EXCEPTION_THROW(type);
#define CHECK_UNDERFLOW(n_values) CHECK(n_values <= intr->stack.length, AZO_EXCEPTION_STACK_UNDERFLOW);
#define TEST_OVERFLOW(n_values) TEST((intr->stack.length + n_values) <= 65536, AZO_EXCEPTION_STACK_OVERFLOW);

#define CHECK_TYPE_EXACT(pos,type) if ((intr->flags & AZO_INTR_FLAG_CHECK_ARGS) && !test_stack_type_exact(intr, ip, pos, type)) return NULL;
#define TEST_TYPE_EXACT(pos,type) if (!test_stack_type_exact(intr, ip, pos, type)) return NULL;
#define TEST_TYPE_IS(pos,type) if (!test_stack_type_is(intr, ip, pos, type)) return NULL;

static const unsigned char *
interpret_EXCEPTION (AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	if (*ip == AZO_TC_EXCEPTION_IF) {
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if(!azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	} else if (*ip == AZO_TC_EXCEPTION_IF_NOT) {
		CHECK_TYPE_EXACT(0, AZ_TYPE_BOOLEAN);
		if (azo_stack_boolean_bw(&intr->stack, 0)) {
			azo_stack_pop (&intr->stack, 1);
			return ip + 5;
		}
	}
	EXCEPTION_THROW(type);
	return NULL;
}

/*
 * AZO_TC_THROW U32:TYPE
 * Pop payload and message (string or none), set the context exception
 * state from the exception registry and unwind
 */
static const unsigned char *
interpret_THROW (AZOInterpreter *intr, const uint8_t *ip)
{
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	CHECK_UNDERFLOW (2);
	unsigned int msg_type = azo_stack_type_bw (&intr->stack, 1);
	if ((msg_type != AZ_TYPE_NONE) && (msg_type != AZ_TYPE_STRING)) {
		/* Bad message value: throw InvalidType instead */
		azo_stack_pop (&intr->stack, 2);
		EXCEPTION_THROW (AZ_EXCEPTION_INVALID_TYPE);
		return ip + 5;
	}
	if (!intr->ctx->az_ctx.exc.type) {
		AZString *msg = NULL;
		if (msg_type == AZ_TYPE_STRING) msg = (AZString *) azo_stack_instance_bw (&intr->stack, 1);
		AZPackedValue payload = { azo_stack_impl_bw (&intr->stack, 0), 0, *azo_stack_value_bw (&intr->stack, 0) };
		azo_context_set_exception_value (intr->ctx, type, msg, &payload, ip);
	}
	azo_stack_pop (&intr->stack, 2);
	if (!(intr->flags & AZO_INTR_FLAG_NO_THROW)) return NULL;
	return ip + 5;
}

/*
* AZO_TC_EXCEPTION_IF_TYPE_IS_NOT pos type
* Throw INVALID_TYPE if element at pos is not type
*/
static const unsigned char *
interpret_type_EXCEPTION (AZOInterpreter *intr, const uint8_t *ip)
{
	uint32_t pos, type;
	memcpy (&pos, ip + 1, 4);
	memcpy (&type, ip + 5, 4);
	CHECK_UNDERFLOW(pos + 1);
	if (az_type_is_a(azo_stack_type_bw(&intr->stack, pos), type)) {
		return ip + 9;
	}
	EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	return NULL;
}

/*
 * AZO_TC_EXCEPTION_IF_NOT_ASSIGNABLE_IMMEDIATE pos type
 * Throw INVALID_TYPE if element at pos is not assignable to type
 */
static const unsigned char *
interpret_exception_if_not_assignable (AZOInterpreter *intr, const uint8_t *ip)
{
	uint32_t pos, type;
	memcpy (&pos, ip + 1, 4);
	memcpy (&type, ip + 5, 4);
	CHECK_UNDERFLOW(pos + 1);
	if (az_type_is_assignable_to (azo_stack_type_bw (&intr->stack, pos), type)) {
		return ip + 9;
	}
	EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	return NULL;
}

#ifdef AZO_TC_HAS_DEBUG
static const unsigned char *
interpret_DEBUG (AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t op;
	memcpy (&op, ip + 1, 4);
	if (*ip == AZO_TC_DEBUG_STACK) {
		fprintf (stderr, "Debug: IPC = %u\n", (unsigned int) (ip - ictx->tcode));
		azo_intepreter_print_stack (intr, stderr);
	} else {
		fprintf (stderr, "Debug: %s\n", ictx->shared_data->entries[op].val.string->str);
	}
	return ip + 5;
}
#endif

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
interpret_REWIND (AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	unsigned int n_values;
	memcpy (&n_values, ip + 1, 4);
	CHECK_UNDERFLOW(n_values);
	intr_rewind(intr, ictx, n_values);
	/* If exception happened during rewind, and we are NOT part of rewinf chain, throw it here */
	/* if (intr->exc.type && !(intr->flags & AZO_INTR_FLAG_NO_THROW)) return NULL; */
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
		// fixme: This is used to reserve variables
		// fixme: The proper way would be to push NULL but reserve space
		//CHECK(!AZ_CLASS_IS_ABSTRACT(klass), AZO_EXCEPTION_INVALID_TYPE);
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
	CHECK(!type || AZ_TYPE_IS_PRIMITIVE(type), AZ_EXCEPTION_INVALID_TYPE);
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
interpret_PUSH_VALUE(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	memcpy (&loc, ip + 1, 4);
	TEST_OVERFLOW(1);
	azo_stack_push_value (&intr->stack, ictx->shared_data->entries[loc].impl, &ictx->shared_data->entries[loc].val);
	return ip + 5;
}

static const unsigned char *
interpret_STORE_VALUE(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	memcpy (&loc, ip + 1, 4);
	unsigned int flags = ip[5];
	CHECK_UNDERFLOW(1);
	if (azo_datablock_set_from_val(ictx->shared_data, loc, azo_stack_impl_bw(&intr->stack, 0), azo_stack_value_bw(&intr->stack, 0), flags) != AZ_OK) {
		EXCEPTION_THROW(AZO_EXCEPTION_NOT_WRITABLE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 6;
}

static const unsigned char *
interpret_PUSH_CAPTURE(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	memcpy (&loc, ip + 1, 4);
	TEST_OVERFLOW(1);
	azo_stack_push_value (&intr->stack, ictx->static_data->entries[loc].impl, &ictx->static_data->entries[loc].val);
	return ip + 5;
}

static const unsigned char *
interpret_STORE_CAPTURE(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	memcpy (&loc, ip + 1, 4);
	unsigned int flags = ip[5];
	CHECK_UNDERFLOW(1);
	if (azo_datablock_set_from_val(ictx->static_data, loc, azo_stack_impl_bw(&intr->stack, 0), azo_stack_value_bw(&intr->stack, 0), flags) != AZ_OK) {
		EXCEPTION_THROW(AZO_EXCEPTION_NOT_WRITABLE);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 6;
}

static const unsigned char *
interpret_JMP_IF_SHARED_SET(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	int32_t raddr;
	memcpy (&raddr, ip + 1, 4);
	memcpy (&loc, ip + 5, 4);
	const unsigned char *dst = ip + 9;
	if (ictx->shared_data->entries[loc].flags & AZO_DATABLOCK_FLAG_SET) dst = ip + 5 + raddr;
	return dst;
}

static const unsigned char *
interpret_JMP_IF_CAPTURE_SET(AZOInterpreter *intr, AZOInterpreterCtx *ictx, const unsigned char *ip)
{
	uint32_t loc;
	int32_t raddr;
	memcpy (&raddr, ip + 1, 4);
	memcpy (&loc, ip + 5, 4);
	const unsigned char *dst = ip + 9;
	if (ictx->static_data->entries[loc].flags & AZO_DATABLOCK_FLAG_SET) dst = ip + 5 + raddr;
	return dst;
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
interpret_TYPE (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int pos, result;
	pos = ip[1];
	CHECK_UNDERFLOW(pos + 1);
	CHECK_TYPE_EXACT(0, AZ_TYPE_UINT32);
	uint32_t type = azo_stack_uint32_bw(&intr->stack, 0);
	switch (ip[0]) {
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
	CHECK_UNDERFLOW(pos + 1);
	memcpy (&type, ip + 2, 4);
	switch (ip[0]) {
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
	CHECK_UNDERFLOW(pos + 1);
	if (ip[0] == AZO_TC_TYPE_OF) {
		type = azo_stack_type_bw (&intr->stack, pos);
		azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE(AZ_TYPE_UINT32), &type);
	} else {
		CHECK_TYPE_EXACT(pos, AZ_TYPE_CLASS);
		AZClass *klass = (AZClass *) azo_stack_instance_bw (&intr->stack, pos);
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
	switch (ip[0]) {
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
interpret_PROMOTE (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int pos, result;
	pos = ip[1];
	CHECK_UNDERFLOW(pos + 1);
	unsigned int type = azo_stack_uint32_bw(&intr->stack, 0);
	/* fixme: Test clamped/rounded */
	if (!azo_stack_convert_bw (&intr->stack, pos, type)) {
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_CONVERSION);
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 2;
}

#define EXCEPTION_THROW_MASKED(type,mask) if(!(intr->flags & mask)) EXCEPTION_THROW(type);

static const unsigned char *
interpret_convert (AZOInterpreter *intr, const uint8_t *ip, unsigned int strict, unsigned int exact)
{
	uint32_t to_type;
	memcpy(&to_type, ip + 1, 4);
	CHECK_UNDERFLOW(1);
	unsigned int from_type = azo_stack_type_bw (&intr->stack, 0);
	if (AZ_TYPE_IS_ARITHMETIC (to_type) && AZ_TYPE_IS_ARITHMETIC (from_type) && (to_type != from_type)) {
		AZValue *from_val = azo_stack_value_bw (&intr->stack, 0);
		AZValue to_val;
		unsigned int flags = 0;
		unsigned int mode = (intr->flags & AZO_INTR_FLAG_WRAP) ? AZO_CALCULATE_MODE_WRAP : AZO_CALCULATE_MODE_SATURATE;
		unsigned int result_code = azo_calculate_convert (mode, to_type, from_type, from_val, &to_val, &flags);
		if (result_code == AZO_CALCULATE_INVALID_TYPE) {
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_CONVERSION);
		}
		if (flags & AZO_CALCULATE_FLAG_MAGNITUDE) {
			if (strict) {
				/* Implicit assignment: magnitude change (clamping) always throws */
				EXCEPTION_THROW(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE);
			} else {
				EXCEPTION_THROW_MASKED(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, AZO_INTR_CHANGE_OF_MAGNITUDE_MASK);
			}
		}
		if (flags & AZO_CALCULATE_FLAG_ROUNDED) {
			if (exact) {
				/* Exact cast qualifier: rounding always throws */
				EXCEPTION_THROW(AZO_EXCEPTION_CHANGE_OF_PRECISION);
			} else {
				EXCEPTION_THROW_MASKED(AZO_EXCEPTION_CHANGE_OF_PRECISION, AZO_INTR_CHANGE_OF_PRECISION_MASK);
			}
		}
		if (flags & AZO_CALCULATE_FLAG_NON_FINITE) {
			EXCEPTION_THROW_MASKED(AZO_EXCEPTION_NOT_FINITE, AZO_INTR_NOT_FINITE_MASK);
		}
		/* Write the converted value back and update the element type */
		*from_val = to_val;
		*azo_stack_impls_bw (&intr->stack, 0) = AZ_IMPL_FROM_TYPE (to_type);
		return ip + 5;
	}
	if (!azo_stack_convert_bw(&intr->stack, 0, to_type)) {
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_CONVERSION);
	}
	return ip + 5;
}

static const unsigned char *
interpret_CONVERT_TYPE (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_convert (intr, ip, 0, 0);
}

static const unsigned char *
interpret_CONVERT_TYPE_ASSIGN (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_convert (intr, ip, 1, 0);
}

static const unsigned char *
interpret_CONVERT_TYPE_EXACT (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_convert (intr, ip, 0, 1);
}

static const unsigned char *
interpret_EQUAL_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type, result;
	const AZValue *lhs, *rhs;
	type = AZ_TYPE_FROM_INDEX(ip[1]);
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	if (AZ_TYPE_IS_PRIMITIVE (type)) {
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
		lhs = azo_stack_value_bw (&intr->stack, 1);
		rhs = azo_stack_value_bw (&intr->stack, 0);
		result = *((unsigned long long *) lhs) == *((unsigned long long *) rhs);
	} else {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_BOOLEAN), &result);
	return ip + 2;
}

static const unsigned char *
interpret_EQUAL (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type, result;
	const void *lhs, *rhs;
	CHECK_UNDERFLOW(2);
	type = azo_stack_type_bw (&intr->stack, 0);
	if (AZ_TYPE_IS_PRIMITIVE (type)) {
		TEST_TYPE_EXACT(1, type);
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
		TEST_TYPE_IS(0, type);
		TEST_TYPE_IS(1, type);
		lhs = azo_stack_value_bw (&intr->stack, 1);
		rhs = azo_stack_value_bw (&intr->stack, 0);
		result = *((unsigned long long *) lhs) == *((unsigned long long *) rhs);
	} else {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
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
	type = AZ_TYPE_FROM_INDEX(ip[1]);
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
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
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_INT32), &result);
	return ip + 2;
}

static const unsigned char *
interpret_COMPARE (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int type;
	const AZValue *lhs, *rhs;
	int result;
	CHECK_UNDERFLOW(2);
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
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
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
	AZValue *val = azo_stack_value_bw(&intr->stack, 0);
	unsigned int flags = 0;
	unsigned int mode = (intr->flags & AZO_INTR_FLAG_WRAP) ? AZO_CALCULATE_MODE_WRAP : AZO_CALCULATE_MODE_SATURATE;
	unsigned int result_code = azo_calculate_negate (mode, type, val, val, &flags);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) EXCEPTION_THROW (AZ_EXCEPTION_INVALID_TYPE);
	if (flags & AZO_CALCULATE_FLAG_OVERFLOW) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_INTEGER_OVERFLOW, AZO_INTR_INTEGER_OVERFLOW_MASK);
	}
	return ip + 1;
}

static const unsigned char *
interpret_PUSH_FLAGS (AZOInterpreter *intr, const unsigned char *ip)
{
	AZValue val;
	val.uint32_v = intr->flags;
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_UINT32), &val);
	return ip + 1;
}

static const unsigned char *
interpret_STORE_FLAGS (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	const AZValue *val = azo_stack_value_bw (&intr->stack, 0);
	intr->flags = val->uint32_v;
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const unsigned char *
interpret_CONJUGATE (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	unsigned int type = azo_stack_type_bw (&intr->stack, 0);
	CHECK((type == AZ_TYPE_COMPLEX_FLOAT) || (type == AZ_TYPE_COMPLEX_DOUBLE), AZ_EXCEPTION_INVALID_TYPE);
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
	CHECK(AZ_TYPE_IS_INTEGRAL(type), AZ_EXCEPTION_INVALID_TYPE);
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
	if (*ip == AZO_TC_LOGICAL_AND) {
		lhs->boolean_v = lhs->boolean_v && rhs->boolean_v;
	} else {
		lhs->boolean_v = lhs->boolean_v || rhs->boolean_v;
	}
	azo_stack_pop (&intr->stack, 1);
	return ip + 1;
}

static const uint8_t *
interpret_binary_typed_calculate (AZOInterpreter *intr, const unsigned char *ip, unsigned int operation)
{
	unsigned int type = AZ_TYPE_FROM_INDEX(ip[1]);
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
	CHECK_UNDERFLOW(2);
	const AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	const AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	AZValue result;
	unsigned int flags = 0;
	unsigned int mode = (intr->flags & AZO_INTR_FLAG_WRAP) ? AZO_CALCULATE_MODE_WRAP : AZO_CALCULATE_MODE_SATURATE;
	unsigned int result_code = azo_calculate_typed (mode, type, lhs, rhs, operation, &result, &flags);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) EXCEPTION_THROW (AZ_EXCEPTION_INVALID_TYPE);
	if (result_code == AZO_CALCULATE_DIVIDE_BY_ZERO) EXCEPTION_THROW (AZO_EXCEPTION_DIVIDE_BY_ZERO);
	if (flags & AZO_CALCULATE_FLAG_MAGNITUDE) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, AZO_INTR_CHANGE_OF_MAGNITUDE_MASK);
	}
	if (flags & AZO_CALCULATE_FLAG_OVERFLOW) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_INTEGER_OVERFLOW, AZO_INTR_INTEGER_OVERFLOW_MASK);
	}
	if (flags & AZO_CALCULATE_FLAG_NON_FINITE) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_NOT_FINITE, AZO_INTR_NOT_FINITE_MASK);
	}
	/* Replace both operands with the result */
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE (type), &result);
	return ip + 2;
}

static const unsigned char *
interpret_ADD_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_typed_calculate (intr, ip, AZO_TERM_ARITHMETIC_PLUS);
}

static const unsigned char *
interpret_binary_calculate (AZOInterpreter *intr, const unsigned char *ip, unsigned int operation)
{
	CHECK_UNDERFLOW(2);
	const AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	unsigned int lhs_type = azo_stack_type_bw (&intr->stack, 1);
	const AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	unsigned int rhs_type = azo_stack_type_bw (&intr->stack, 0);
	AZValue result;
	unsigned int result_type;
	unsigned int flags;
	unsigned int mode = (intr->flags & AZO_INTR_FLAG_WRAP) ? AZO_CALCULATE_MODE_WRAP : AZO_CALCULATE_MODE_SATURATE;
	unsigned int result_code = azo_calculate_binary (mode, lhs_type, lhs, rhs_type, rhs, operation, &result_type, &result, &flags);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) EXCEPTION_THROW (AZ_EXCEPTION_INVALID_TYPE);
	if (result_code == AZO_CALCULATE_DIVIDE_BY_ZERO) EXCEPTION_THROW (AZO_EXCEPTION_DIVIDE_BY_ZERO);
	if (flags & AZO_CALCULATE_FLAG_MAGNITUDE) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_CHANGE_OF_MAGNITUDE, AZO_INTR_CHANGE_OF_MAGNITUDE_MASK);
	}
	if (flags & AZO_CALCULATE_FLAG_OVERFLOW) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_INTEGER_OVERFLOW, AZO_INTR_INTEGER_OVERFLOW_MASK);
	}
	if (flags & AZO_CALCULATE_FLAG_NON_FINITE) {
		EXCEPTION_THROW_MASKED(AZO_EXCEPTION_NOT_FINITE, AZO_INTR_NOT_FINITE_MASK);
	}
	/* Replace both operands with the result */
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE (result_type), &result);
	return ip + 1;
}

static const unsigned char *
interpret_ADD (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_PLUS);
}

static const uint8_t *
interpret_SUBTRACT_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_typed_calculate (intr, ip, AZO_TERM_ARITHMETIC_MINUS);
}

static const uint8_t *
interpret_SUBTRACT (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_MINUS);
}

static const uint8_t *
interpret_MULTIPLY_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_typed_calculate (intr, ip, AZO_TERM_ARITHMETIC_STAR);
}

static const unsigned char *
interpret_MULTIPLY (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_STAR);
}

static const uint8_t *
interpret_DIVIDE_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_typed_calculate (intr, ip, AZO_TERM_ARITHMETIC_SLASH);
}

static const uint8_t *
interpret_DIVIDE (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_SLASH);
}

static const uint8_t *
interpret_MODULO_TYPED (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_typed_calculate (intr, ip, AZO_TERM_ARITHMETIC_PERCENT);
}

static const uint8_t *
interpret_MODULO (AZOInterpreter *intr, const uint8_t *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_PERCENT);
}

static const unsigned char *
interpret_binary_shift (AZOInterpreter *intr, const unsigned char *ip, unsigned int operation)
{
	CHECK_UNDERFLOW(2);
	unsigned int lhs_type = azo_stack_type_bw (&intr->stack, 1);
	unsigned int rhs_type = azo_stack_type_bw (&intr->stack, 0);
	const AZValue *lhs = azo_stack_value_bw (&intr->stack, 1);
	const AZValue *rhs = azo_stack_value_bw (&intr->stack, 0);
	AZValue result;
	unsigned int result_type;
	unsigned int result_code = azo_calculate_shift (lhs_type, lhs, rhs_type, rhs, operation, &result_type, &result);
	if (result_code == AZO_CALCULATE_INVALID_TYPE) EXCEPTION_THROW (AZ_EXCEPTION_INVALID_TYPE);
	/* Replace both operands with the result */
	azo_stack_pop (&intr->stack, 2);
	azo_stack_push_value (&intr->stack, AZ_IMPL_FROM_TYPE (result_type), &result);
	return ip + 1;
}

static const unsigned char *
interpret_BITWISE_AND (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_AND);
}

static const unsigned char *
interpret_BITWISE_OR (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_OR);
}

static const unsigned char *
interpret_BITWISE_XOR (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_calculate (intr, ip, AZO_TERM_ARITHMETIC_CARET);
}

static const unsigned char *
interpret_SHIFT_LEFT (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_shift (intr, ip, AZO_TERM_ARITHMETIC_SHIFT_LEFT);
}

static const unsigned char *
interpret_SHIFT_RIGHT (AZOInterpreter *intr, const unsigned char *ip)
{
	return interpret_binary_shift (intr, ip, AZO_TERM_ARITHMETIC_SHIFT_RIGHT);
}

static const unsigned char *
interpret_MIN_MAX_TYPED (AZOInterpreter *intr, const unsigned char *ip)
{
	unsigned int remove;
	unsigned int type = AZ_TYPE_FROM_INDEX(ip[1]);
	CHECK_TYPE_EXACT(0, type);
	CHECK_TYPE_EXACT(1, type);
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
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	if (ip[0] == AZO_TC_MAX_TYPED) remove = !remove;
	azo_stack_remove (&intr->stack, intr->stack.length - 1 - remove, 1);
	return ip + 2;
}

static const unsigned char *
interpret_GET_INTERFACE_IMMEDIATE (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	uint32_t type;
	memcpy (&type, ip + 1, 4);
	CHECK(AZ_IMPL_IS_BLOCK(azo_stack_impl_bw(&intr->stack, 0)), AZ_EXCEPTION_INVALID_TYPE);
	CHECK(AZ_TYPE_IS_INTERFACE(type), AZ_EXCEPTION_INVALID_TYPE);
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
interpret_INVOKE (AZOInterpreter *intr, AZOInterpreterCtx *ictx, const uint8_t *ip)
{
	unsigned int pos = ip[1];
	CHECK_UNDERFLOW(pos);
	AZFunctionInstance *func_inst;
	const AZFunctionImplementation *func_impl = (const AZFunctionImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, pos), azo_stack_instance_bw (&intr->stack, pos), AZ_TYPE_FUNCTION, (void **) &func_inst);
	if (!func_impl) EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	const AZFunctionSignature *sig = az_function_get_signature (func_impl, func_inst);
	for (unsigned int i = 0; i < sig->n_args; i++) {
		if (!azo_stack_convert_bw (&intr->stack, sig->n_args - 1 - i, sig->arg_types[i])) {
			EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
		}
	}
	unsigned int prev = azo_interpreter_push_frame(intr, pos);
	AZPackedValue64 ret_val = {0};
	az_function_invoke (func_impl, func_inst,
		azo_stack_impls_bw(&intr->stack, sig->n_args - 1),
		(const AZValue **)azo_stack_values_bw (&intr->stack, sig->n_args - 1),
		&ret_val.impl, &ret_val.v, &intr->ctx->az_ctx);
	unsigned int new_prev = azo_interpreter_pop_frame(intr, ictx);
	assert(new_prev == prev);
	/* Pop function and the arguments */
	azo_stack_pop(&intr->stack, 1);
	azo_stack_push_value_transfer (&intr->stack, ret_val.impl, &ret_val.v);
	/* On exception run_code unwinds: the trace records the call ip and the
	 * exception keeps the ip set by its throwing site */
	if(intr->ctx->az_ctx.exc.type) {
		return NULL;
	}
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
	azo_stack_pop(&intr->stack, 1);
	return NULL;
}

static const uint8_t *
interpret_CLOSURE(AZOInterpreter *intr, const unsigned char *ip)
{
	uint32_t n_vals;
	memcpy (&n_vals, ip + 1, 4);
	CHECK_UNDERFLOW(n_vals + 1);
	CHECK_TYPE_EXACT(n_vals, AZO_TYPE_PROGRAM);

	AZOProgram *prog = (AZOProgram *) azo_stack_instance_bw(&intr->stack, n_vals);
	AZOCompiledFunction *cfunc = azo_compiled_function_new(prog);
	for (unsigned int i = 0; i < n_vals; i++) {
		const AZImplementation *arg_impl = azo_stack_impl_bw(&intr->stack, n_vals - 1 - i);
		void *arg_inst = azo_stack_instance_bw(&intr->stack, n_vals - 1 - i);
		azo_compiled_function_bind (cfunc, i, arg_impl, arg_inst);
	}
	azo_stack_pop(&intr->stack, n_vals + 1);
	azo_stack_push_instance(&intr->stack, (const AZImplementation *) cfunc->object.klass, cfunc);
	az_object_unref((AZObject *) cfunc);

	return ip + 5;
}

static const unsigned char *
interpret_NEW_ARRAY (AZOInterpreter *intr, const unsigned char *ip)
{
	CHECK_UNDERFLOW(1);
	unsigned int size;
	AZValueArray *varray;
	AZClass *varray_class;
	CHECK(azo_stack_type_bw(&intr->stack, 0) == AZ_TYPE_UINT32, AZ_EXCEPTION_INVALID_TYPE);
	size = *((unsigned int *) azo_stack_value_bw (&intr->stack, 0));
	azo_stack_pop (&intr->stack, 1);
	varray = az_value_array_new (size);
	varray_class = az_type_get_class (AZ_TYPE_VALUE_ARRAY);
	azo_stack_push_instance (&intr->stack, &varray_class->impl, varray);
	az_reference_unref ((AZReferenceClass *) varray_class, &varray->reference);
	return ip + 1;
}

static const unsigned char *
interpret_LOAD_ARRAY_ELEMENT (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int idx;
	CHECK_UNDERFLOW(2);
	AZPackedValue64 val = { 0 };
	const AZImplementation *obj_impl = azo_stack_impl_bw (&intr->stack, 1);
	const AZImplementation *idx_impl = azo_stack_impl_bw (&intr->stack, 0);
	if (!obj_impl || !idx_impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_NULL_DEREFERENCE);
	}
	void *list_inst;
	AZListImplementation *list_impl = (AZListImplementation *) az_instance_get_interface(obj_impl, azo_stack_instance_bw(&intr->stack, 1), AZ_TYPE_LIST, (void **) &list_inst);
	if (!list_impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	if (!convert_to_u32 (intr, &idx, idx_impl, azo_stack_value_bw(&intr->stack, 0), ip)) {
		EXCEPTION_THROW(AZO_EXCEPTION_INVALID_CONVERSION);
	}
	if (idx >= az_collection_get_size(&list_impl->collection_impl, list_inst)) {
		EXCEPTION_THROW(AZ_EXCEPTION_OUT_OF_BOUNDS);
	}
	val.impl = az_list_get_element(list_impl, list_inst, idx, &val.v.value, 64);
	azo_stack_pop(&intr->stack, 1);
	azo_stack_push_value_transfer(&intr->stack, val.impl, &val.v);
	return ip + 1;
}

static const unsigned char *
interpret_WRITE_ARRAY_ELEMENT (AZOInterpreter *intr, const uint8_t *ip)
{
	AZValueArray *varray;
	unsigned int idx;
	AZPackedValue val = { 0 };
	if (intr->flags & AZO_INTR_FLAG_CHECK_ARGS) {
		if (!az_type_is_a (azo_stack_type_bw (&intr->stack, 2), AZ_TYPE_VALUE_ARRAY)) {
			EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
		}
		varray = *((AZValueArray **) azo_stack_value_bw (&intr->stack, 2));
		if (!convert_to_u32 (intr, &idx, azo_stack_impl_bw (&intr->stack, 1), azo_stack_value_bw (&intr->stack, 1), ip)) {
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_CONVERSION);
		}
		if (idx >= varray->size) {
			EXCEPTION_THROW(AZ_EXCEPTION_OUT_OF_BOUNDS);
		}
		az_packed_value_set_from_impl_value (&val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
	} else {
		varray = *((AZValueArray **) azo_stack_value_bw (&intr->stack, 2));
		idx = *((unsigned int *) azo_stack_value_bw (&intr->stack, 1));
		az_packed_value_set_from_impl_value (&val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
	}
	az_value_array_set_element_from_val (varray, idx, val.impl, &val.v);
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
	if ((intr->vals[0].impl = azo_context_lookup (intr->ctx, key, &intr->vals[0].v.value, 64))) {
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value_transfer (&intr->stack, intr->vals[0].impl, &intr->vals[0].v.value);
		intr->vals[0].impl = NULL;
	} else {
		azo_stack_pop (&intr->stack, 1);
		azo_stack_push_value (&intr->stack, NULL, NULL);
	}
	return ip + 1;
}

/* Instance, String -> Value */

static const unsigned char *
interpret_GET_PROPERTY (AZOInterpreter *intr, const uint8_t *ip)
{
	const AZImplementation *impl;
	void *inst;
	AZString *key;
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(2);
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	/* fixme: Implement automatic unboxing (in stack?) or not? */
	impl = azo_stack_impl_bw (&intr->stack, 1);
	if (!impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_NULL_DEREFERENCE);
	}
	inst = azo_stack_instance_bw (&intr->stack, 1);
	if (impl == &AZBoxedInterfaceKlass.klass.impl) {
		az_boxed_interface_unbox(&impl, &inst);
	}
	intr->vals[0].impl = NULL;
	/* fixme: We should support class properties on null objects probably */
	if (!inst) {
		EXCEPTION_THROW(AZ_EXCEPTION_NULL_DEREFERENCE);
	}

	/* Try simple property */
	if (az_instance_get_property_by_key (impl, inst, key->str, &intr->vals[0].impl, &intr->vals[0].v, 64) == AZ_OK) {
		/* Ordinary property */
		azo_stack_pop (&intr->stack, 2);
		azo_stack_push_value_transfer (&intr->stack, intr->vals[0].packed_val.impl, &intr->vals[0].packed_val.v);
		return ip + 1;
	}
	/* Try interface */
	if ((intr->vals[0].impl = azo_context_lookup (intr->ctx, key, &intr->vals[0].v.value, 64)) && (AZ_IMPL_TYPE(intr->vals[0].impl) == AZ_TYPE_CLASS)) {
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
			if (AZ_FIELD_SPEC(field) == AZ_FIELD_INSTANCE) {
				/* Field is instance */
			} else {
				if (az_instance_get_property_by_id (sub_class, AZ_CLASS_FROM_IMPL(prop_impl), prop_impl, NULL, idx, &intr->vals[0].impl, &intr->vals[0].v.value, 64) == AZ_OK) {
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
interpret_GET_FUNCTION (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int n_args = ip[1];
	CHECK_TYPE_EXACT(n_args, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(n_args + 2);
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
		EXCEPTION_THROW(AZ_EXCEPTION_NULL_DEREFERENCE);
	}
	const AZClass *def_class;
	const AZImplementation *sub_impl;
	void *sub_inst;
	int idx = az_class_lookup_function(AZ_CLASS_FROM_IMPL(impl), impl, azo_stack_instance_bw (&intr->stack, n_args + 1), key, (AZFunctionSignature *) &sig, &def_class, &sub_impl, &sub_inst);
	if (idx >= 0) {
		AZPackedValue64 val;
		if (az_instance_get_property_by_id(def_class, AZ_CLASS_FROM_IMPL(sub_impl), sub_impl, sub_inst, idx, &val.impl, &val.v.value, 64) == AZ_OK) {
			azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v.value);
			return ip + 2;
		}
	}
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 2;
}

static const unsigned char *
interpret_SET_PROPERTY (AZOInterpreter *intr, const uint8_t *ip)
{
	unsigned int result = 0;
	/* INSTANCE STRING VALUE */
	CHECK_TYPE_EXACT(1, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(3);
	AZString *key = (AZString *) azo_stack_instance_bw (&intr->stack, 1);
	const AZImplementation *impl = azo_stack_impl_bw (&intr->stack, 2);
	if (!impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_NULL_DEREFERENCE);
	}
	void *inst = azo_stack_instance_bw (&intr->stack, 2);
	if (impl == &AZBoxedInterfaceKlass.klass.impl) {
		az_boxed_interface_unbox(&impl, &inst);
	}
	AZClass *klass = AZ_CLASS_FROM_IMPL(impl);
	const AZClass *def_class;
	const AZImplementation *sub_impl;
	void *sub_inst;
	int idx = az_class_lookup_property (klass, impl, inst, key, &def_class, &sub_impl, &sub_inst);
	if (idx >= 0) {
		AZField *prop = &def_class->props_self[idx];
		unsigned int type = azo_stack_type_bw (&intr->stack, 0);
		if (!type && (AZ_TYPE_IS_REFERENCE(prop->type) || AZ_TYPE_IS_INTERFACE(prop->type))) {
			result = !az_instance_set_property_by_id(def_class, AZ_CLASS_FROM_IMPL(sub_impl), sub_impl, sub_inst, idx, NULL, NULL);
			if (result) {
				EXCEPTION_THROW(AZ_EXCEPTION_INVALID_VALUE);
			}
		} else if (!az_type_is_assignable_to (type, prop->type)) {
			/* fixme: No need for double copy, unless we want to use packed value conversion */
			intr->vals[0].impl = NULL;
			intr->vals[1].impl = NULL;
			az_packed_value_set_from_impl_value (&intr->vals[0].packed_val, azo_stack_impl_bw (&intr->stack, 0), azo_stack_value_bw (&intr->stack, 0));
			if (az_packed_value_convert (&intr->vals[1].packed_val, prop->type, &intr->vals[0].packed_val)) {
				result = !az_instance_set_property_by_id (def_class, AZ_CLASS_FROM_IMPL(sub_impl), sub_impl, sub_inst, idx, intr->vals[1].impl, az_packed_value_get_inst (&intr->vals[1].packed_val));
				az_packed_value_clear (&intr->vals[1].packed_val);
			}
			az_packed_value_clear (&intr->vals[0].packed_val);
		} else {
			result = !az_instance_set_property_by_id (def_class, AZ_CLASS_FROM_IMPL(sub_impl), sub_impl, sub_inst, idx, azo_stack_impl_bw (&intr->stack, 0), azo_stack_instance_bw (&intr->stack, 0));
			if (!result) {
				EXCEPTION_THROW(AZ_EXCEPTION_INVALID_VALUE);
			}
		}
	} else {
		//7azo_exception_set (&intr->exc, AZO_EXCEPTION_INVALID_PROPERTY, 1UL << AZO_EXCEPTION_INVALID_PROPERTY, ip);
		//return NULL;
	}
	if (result) azo_stack_pop (&intr->stack, 3);
	azo_stack_push_value (&intr->stack, (AZImplementation *) az_type_get_class (AZ_TYPE_BOOLEAN), &result);
	return ip + 1;
}

/* Class, String -> Value */

static const unsigned char *
interpret_GET_STATIC_PROPERTY (AZOInterpreter *intr, const uint8_t *ip)
{
	AZString *key;
	AZClass *klass;
	int idx;
	const AZClass *sub_class;
	const AZImplementation *prop_impl;
	CHECK_TYPE_EXACT(1, AZ_TYPE_CLASS);
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(2);
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	klass = (AZClass *) azo_stack_instance_bw (&intr->stack, 1);

	idx = az_class_lookup_property (klass, &klass->impl, NULL, key, &sub_class, &prop_impl, NULL);
	if (idx < 0) {
		AZField *field = &sub_class->props_self[idx];
		if (AZ_FIELD_SPEC(field) == AZ_FIELD_INSTANCE) {
			/* No such field or field is instance */
		} else {
			if (az_instance_get_property_by_id (sub_class, AZ_CLASS_FROM_IMPL(prop_impl), prop_impl, NULL, idx, &intr->vals[0].impl, &intr->vals[0].v.value, 64) == AZ_OK) {
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
	AZFunctionSignature32 sig;
	unsigned int n_args = ip[1];
	CHECK_TYPE_EXACT(n_args + 1, AZ_TYPE_CLASS);
	CHECK_TYPE_EXACT(n_args, AZ_TYPE_STRING);
	AZClass *klass = (AZClass *) azo_stack_instance_bw (&intr->stack, n_args + 1);
	AZString *key = (AZString *) azo_stack_instance_bw (&intr->stack, n_args);
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
		if (az_instance_get_property_by_id(def_class, klass, NULL, NULL, idx, &val.impl, &val.v.value, 64) == AZ_OK) {
			azo_stack_push_value_transfer (&intr->stack, val.impl, &val.v.value);
			return ip + 2;
		}
	}
	azo_stack_push_value (&intr->stack, NULL, NULL);
	return ip + 2;
}

static const unsigned char *
interpret_GET_ATTRIBUTE (AZOInterpreter *intr, const unsigned char *ip)
{
	AZString *key;
	void *attrd_inst;
	const AZAttribDictImplementation *attrd_impl;
	unsigned int flags;
	CHECK_TYPE_EXACT(0, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(2);
	CHECK(az_type_implements(azo_stack_type_bw(&intr->stack, 1), AZ_TYPE_ATTRIBUTE_DICT), AZ_EXCEPTION_INVALID_TYPE);
	key = (AZString *) azo_stack_instance_bw (&intr->stack, 0);
	attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 1), azo_stack_instance_bw (&intr->stack, 1), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	if (!attrd_impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	intr->vals[0].impl = az_attrib_dict_lookup (attrd_impl, attrd_inst, key, &intr->vals[0].v.value, 64, &flags);
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
interpret_SET_ATTRIBUTE (AZOInterpreter *intr, const uint8_t *ip)
{
	CHECK_TYPE_EXACT(1, AZ_TYPE_STRING);
	CHECK_UNDERFLOW(3);
	CHECK(az_type_implements(azo_stack_type_bw(&intr->stack, 2), AZ_TYPE_ATTRIBUTE_DICT), AZ_EXCEPTION_INVALID_TYPE);
	AZString *key = (AZString *) azo_stack_instance_bw (&intr->stack, 1);
	void *attrd_inst;
	const AZAttribDictImplementation *attrd_impl = (const AZAttribDictImplementation *) az_instance_get_interface (azo_stack_impl_bw (&intr->stack, 2), azo_stack_instance_bw (&intr->stack, 2), AZ_TYPE_ATTRIBUTE_DICT, &attrd_inst);
	if (!attrd_impl) {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_TYPE);
	}
	const AZImplementation *val_impl = azo_stack_impl_bw (&intr->stack, 0);
	void *val_inst = azo_stack_instance_bw (&intr->stack, 0);
	if (!az_attrib_dict_set (attrd_impl, attrd_inst, key, val_impl, val_inst, 0)) {
		EXCEPTION_THROW(AZ_EXCEPTION_INVALID_VALUE);
	}
	azo_stack_pop (&intr->stack, 3);
	return ip + 1;
}

/* End new stack methods */

const uint8_t *
azo_interpreter_interpret_tc (AZOInterpreter *intr, AZOInterpreterCtx *ictx, const uint8_t *ip)
{
switch (*ip) {
	case AZO_TC_EXCEPTION:
		case AZO_TC_EXCEPTION_IF:
		case AZO_TC_EXCEPTION_IF_NOT:
			ip = interpret_EXCEPTION (intr, ip);
			break;
		case AZO_TC_THROW:
			ip = interpret_THROW (intr, ip);
			break;
		case AZO_TC_EXCEPTION_IF_TYPE_IS_NOT:
			ip = interpret_type_EXCEPTION (intr, ip);
			break;
		case AZO_TC_EXCEPTION_IF_NOT_ASSIGNABLE_IMMEDIATE:
			ip = interpret_exception_if_not_assignable (intr, ip);
			break;

#ifdef AZO_TC_HAS_DEBUG
		case AZO_TC_DEBUG_STACK:
		case AZO_TC_DEBUG_STR:
			ip = interpret_DEBUG (intr, ictx, ip);
			break;
#endif

		/* Stack management */
		case AZO_TC_POP:
			ip = interpret_POP(intr, ip);
			break;
		case AZO_TC_REWIND:
			ip = interpret_REWIND(intr, ictx, ip);
			break;
		case AZO_TC_REMOVE:
			ip = interpret_REMOVE (intr, ip);
			break;
		case AZO_TC_PUSH_EMPTY:
			ip = interpret_PUSH_EMPTY (intr, ip);
			break;
		case AZO_TC_PUSH_IMMEDIATE:
			ip = interpret_PUSH_IMMEDIATE (intr, ip);
			break;
		case AZO_TC_PUSH_VALUE:
			ip = interpret_PUSH_VALUE (intr, ictx, ip);
			break;
		case AZO_TC_STORE_VALUE:
			ip = interpret_STORE_VALUE (intr, ictx,ip);
			break;
		case AZO_TC_PUSH_CAPTURE:
			ip = interpret_PUSH_CAPTURE (intr, ictx, ip);
			break;
		case AZO_TC_STORE_CAPTURE:
			ip = interpret_STORE_CAPTURE (intr, ictx, ip);
			break;
		case AZO_TC_JMP_IF_SHARED_SET:
			ip = interpret_JMP_IF_SHARED_SET (intr, ictx, ip);
			break;
		case AZO_TC_JMP_IF_CAPTURE_SET:
			ip = interpret_JMP_IF_CAPTURE_SET (intr, ictx, ip);
			break;
		case AZO_TC_DUPLICATE:
			ip = interpret_DUPLICATE (intr, ip);
			break;
		case AZO_TC_DUPLICATE_FRAME:
			ip = interpret_DUPLICATE_FRAME (intr, ip);
			break;
		case AZO_TC_EXCHANGE:
			ip = interpret_EXCHANGE (intr, ip);
			break;
		case AZO_TC_EXCHANGE_FRAME:
			ip = interpret_EXCHANGE_FRAME (intr, ip);
			break;
		case AZO_TC_PUSH_FLAGS:
			ip = interpret_PUSH_FLAGS (intr, ip);
			break;
		case AZO_TC_STORE_FLAGS:
			ip = interpret_STORE_FLAGS (intr, ip);
			break;

		/* Type tests */
		case AZO_TC_TYPE_EQUALS:
		case AZO_TC_TYPE_IS:
		case AZO_TC_TYPE_IS_SUPER:
		case AZO_TC_TYPE_IMPLEMENTS:
			ip = interpret_TYPE (intr, ip);
			break;
		case AZO_TC_TYPE_EQUALS_IMMEDIATE:
		case AZO_TC_TYPE_IS_IMMEDIATE:
		case AZO_TC_TYPE_IS_SUPER_IMMEDIATE:
		case AZO_TC_TYPE_IMPLEMENTS_IMMEDIATE:
			ip = interpret_TYPE_IMMEDIATE (intr, ip);
			break;
		case AZO_TC_TYPE_OF:
		case AZO_TC_TYPE_OF_CLASS:
			ip = interpret_TYPE_OF (intr, ip);
			break;

		/* Jumps */
		case JMP_32:
			ip = interpret_JMP_32 (intr, ip);
			break;
		case JMP_32_IF:
		case JMP_32_IF_NOT:
		case JMP_32_IF_ZERO:
		case JMP_32_IF_POSITIVE:
		case JMP_32_IF_NEGATIVE:
			ip = interpret_JMP_32_COND (intr, ip);
			break;

		/* Conversions */
		case AZO_TC_PROMOTE:
			ip = interpret_PROMOTE (intr, ip);
			break;
		case AZO_TC_CONVERT_TYPE:
			ip = interpret_CONVERT_TYPE (intr, ip);
			break;
		case AZO_TC_CONVERT_TYPE_ASSIGN:
			ip = interpret_CONVERT_TYPE_ASSIGN (intr, ip);
			break;
		case AZO_TC_CONVERT_TYPE_EXACT:
			ip = interpret_CONVERT_TYPE_EXACT (intr, ip);
			break;

		/* Comparisons */
		case AZO_TC_EQUAL_TYPED:
			ip = interpret_EQUAL_TYPED (intr, ip);
			break;
		case EQUAL:
			ip = interpret_EQUAL (intr, ip);
			break;
		case AZO_TC_COMPARE_TYPED:
			ip = interpret_COMPARE_TYPED (intr, ip);
			break;
		case COMPARE:
			ip = interpret_COMPARE (intr, ip);
			break;

		case AZO_TC_LOGICAL_NOT:
			ip = interpret_LOGICAL_NOT (intr, ip);
			break;
		case AZO_TC_NEGATE:
			ip = interpret_NEGATE (intr, ip);
			break;
		case AZO_TC_CONJUGATE:
			ip = interpret_CONJUGATE (intr, ip);
			break;
		case AZO_TC_BITWISE_NOT:
			ip = interpret_BITWISE_NOT (intr, ip);
			break;

		case AZO_TC_LOGICAL_AND:
		case AZO_TC_LOGICAL_OR:
			ip = interpret_LOGICAL_BINARY (intr, ip);
			break;
		case AZO_TC_ADD_TYPED:
			ip = interpret_ADD_TYPED (intr, ip);
			break;
		case AZO_TC_SUBTRACT_TYPED:
			ip = interpret_SUBTRACT_TYPED (intr, ip);
			break;
		case AZO_TC_MULTIPLY_TYPED:
			ip = interpret_MULTIPLY_TYPED (intr, ip);
			break;
		case AZO_TC_DIVIDE_TYPED:
			ip = interpret_DIVIDE_TYPED (intr, ip);
			break;
		case AZO_TC_MODULO_TYPED:
			ip = interpret_MODULO_TYPED (intr, ip);
			break;
		case AZO_TC_ADD:
			ip = interpret_ADD (intr, ip);
			break;
		case AZO_TC_SUBTRACT:
			ip = interpret_SUBTRACT (intr, ip);
			break;
		case AZO_TC_MULTIPLY:
			ip = interpret_MULTIPLY (intr, ip);
			break;
		case AZO_TC_DIVIDE:
			ip = interpret_DIVIDE (intr, ip);
			break;
		case AZO_TC_MODULO:
			ip = interpret_MODULO (intr, ip);
			break;
		case AZO_TC_BITWISE_AND:
			ip = interpret_BITWISE_AND (intr, ip);
			break;
		case AZO_TC_BITWISE_OR:
			ip = interpret_BITWISE_OR (intr, ip);
			break;
		case AZO_TC_BITWISE_XOR:
			ip = interpret_BITWISE_XOR (intr, ip);
			break;
		case AZO_TC_SHIFT_LEFT:
			ip = interpret_SHIFT_LEFT (intr, ip);
			break;
		case AZO_TC_SHIFT_RIGHT:
			ip = interpret_SHIFT_RIGHT (intr, ip);
			break;

		case AZO_TC_MIN_TYPED:
		case AZO_TC_MAX_TYPED:
			ip = interpret_MIN_MAX_TYPED (intr, ip);
			break;

		case AZO_TC_GET_INTERFACE_IMMEDIATE:
			ip = interpret_GET_INTERFACE_IMMEDIATE (intr, ip);
			break;
		case AZO_TC_INVOKE:
			ip = interpret_INVOKE (intr, ictx, ip);
			break;
		case AZO_TC_RETURN:
			ip = interpret_RETURN (intr, ip);
			break;
		case AZO_TC_RETURN_VALUE:
			ip = interpret_RETURN_VALUE (intr, ip);
			break;
		case AZO_TC_CLOSURE:
			ip = interpret_CLOSURE (intr, ip);
			break;

		case NEW_ARRAY:
			ip = interpret_NEW_ARRAY (intr, ip);
			break;
		case LOAD_ARRAY_ELEMENT:
			ip = interpret_LOAD_ARRAY_ELEMENT (intr, ip);
			break;
		case WRITE_ARRAY_ELEMENT:
			ip = interpret_WRITE_ARRAY_ELEMENT (intr, ip);
			break;

		case AZO_TC_GET_GLOBAL:
			ip = interpret_GET_GLOBAL (intr, ip);
			break;
		case AZO_TC_GET_PROPERTY:
			ip = interpret_GET_PROPERTY (intr, ip);
			break;
		case AZO_TC_GET_FUNCTION:
			ip = interpret_GET_FUNCTION (intr, ip);
			break;
		case AZO_TC_SET_PROPERTY:
			ip = interpret_SET_PROPERTY (intr, ip);
			break;
		case AZO_TC_GET_STATIC_PROPERTY:
			ip = interpret_GET_STATIC_PROPERTY (intr, ip);
			break;
		case AZO_TC_GET_STATIC_FUNCTION:
			ip = interpret_GET_STATIC_FUNCTION (intr, ip);
			break;
		case AZO_TC_GET_ATTRIBUTE:
			ip = interpret_GET_ATTRIBUTE (intr, ip);
			break;
		case AZO_TC_SET_ATTRIBUTE:
			ip = interpret_SET_ATTRIBUTE (intr, ip);
			break;

		case NOP:
			ip += 1;
			break;

		default:
			EXCEPTION_THROW(AZO_EXCEPTION_INVALID_OPCODE);
	}
	return ip;
}

void
azo_interpreter_print_exception (AZOInterpreter *intr, AZOInterpreterCtx *ictx, FILE *pf)
{
	unsigned char b[1024];
	unsigned int ip = intr->ctx->exc_ip - ictx->tcode;
	fprintf (pf, "Fatal exception: %s\n", intr->ctx->az_ctx.exc.msg->str);
	azo_debug_print_term (ictx->debug, ip, pf);
	fprintf (pf, "Position: %d %d\n", ip, ictx->tcode[ip]);
	azo_bc_print_instruction (b, 256, ictx->tcode, ip, ictx->tcode_len);
	fprintf (pf, "%s\n", b);
	azo_intepreter_print_stack (intr, pf);
	fprintf (pf, "\n");
}

void
azo_interpreter_run_frame(AZOInterpreter *intr, AZOInterpreterCtx *ictx)
{
	const uint8_t *ipc = ictx->tcode;
	const uint8_t *end = ictx->tcode + ictx->tcode_len;
	assert(!intr->ctx->az_ctx.exc.type);

	azo_interpreter_run_code(intr, ictx, ipc, end);
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
