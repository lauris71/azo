#define __AZO_STACK_TEST_C__

/*
* Stress test for the azo value stack
*
* Pushes large numbers of mixed values (primitives, references, large values,
* boxed values) and performs exchanges, removes, rolls and duplicates, while
* verifying after every operation:
* - element impls and contents match a shadow model
* - boxed values are never stored on the stack (transparently unboxed)
* - large value data slots remain below the data high-water mark
* - free data space is contracted on pop/remove (from the top)
*/

#include <inttypes.h>
#include <string.h>

#include <az/base.h>
#include <az/boxed-value.h>
#include <az/complex.h>
#include <az/extend.h>
#include <az/string.h>
#include <az/types.h>
#include <az/value.h>

#include <azo/stack.h>

#include "unity/unity.h"

/* Deterministic PRNG */
static uint32_t rng_state;
static uint32_t
rnd (void)
{
	rng_state = rng_state * 1664525 + 1013904223;
	return rng_state >> 8;
}

/* Registered struct types (final values of 12..48 bytes) */
#define NUM_STRUCT_TYPES 5
static unsigned int struct_types[NUM_STRUCT_TYPES];
static const unsigned int struct_sizes[NUM_STRUCT_TYPES] = { 12, 16, 24, 32, 48 };
static AZClass *struct_klasses[NUM_STRUCT_TYPES];
static unsigned int struct_types_registered = 0;

static void
register_struct_types (void)
{
	if (struct_types_registered) return;
	for (unsigned int i = 0; i < NUM_STRUCT_TYPES; i++) {
		uint8_t name[32];
		snprintf ((char *) name, 32, "stack_test_struct_%u", struct_sizes[i]);
		struct_klasses[i] = az_register_type (&struct_types[i], name, AZ_TYPE_STRUCT, sizeof (AZClass), struct_sizes[i], AZ_FLAG_FINAL, 0, 0, NULL, NULL, NULL);
		TEST_ASSERT (struct_klasses[i]->instance_size == struct_sizes[i]);
	}
	struct_types_registered = 1;
}

/* Shadow model entry */
typedef struct {
	const AZImplementation *impl;
	union {
		uint8_t bytes[64];	/* value content */
		void *ptr;		/* block instance */
	};
	/* The model owns a string reference (transfers clear it) */
	AZString *owned;
} ModelEntry;

#define MODEL_SIZE 512
static ModelEntry model[MODEL_SIZE];

static void
model_drop (unsigned int first, unsigned int n)
{
	for (unsigned int i = first; i < first + n; i++) {
		if (model[i].owned) az_string_unref (model[i].owned);
		model[i].owned = NULL;
	}
}

static void
model_remove (unsigned int first, unsigned int n, unsigned int *len)
{
	model_drop (first, n);
	memmove (&model[first], &model[first + n], (*len - first - n) * sizeof (ModelEntry));
	*len -= n;
}

static void
model_exchange (unsigned int pos, unsigned int *len)
{
	ModelEntry e = model[pos];
	model[pos] = model[*len - 1];
	model[*len - 1] = e;
}

static void
model_roll (unsigned int pos, unsigned int *len)
{
	ModelEntry e = model[*len - 1];
	memmove (&model[pos + 1], &model[pos], (*len - 1 - pos) * sizeof (ModelEntry));
	model[pos] = e;
}

/* Fill an entry with a random value of the given kind (0..10) */
static void
model_fill (ModelEntry *e, unsigned int kind, uint32_t tag)
{
	e->owned = NULL;
	switch (kind) {
	case 0: /* int32 (inline) */
		e->impl = AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32);
		*((int32_t *) e->bytes) = (int32_t) tag;
		break;
	case 1: /* uint64 (inline) */
		e->impl = AZ_IMPL_FROM_TYPE (AZ_TYPE_UINT64);
		*((uint64_t *) e->bytes) = 0x100000000ULL + tag;
		break;
	case 2: /* double (inline) */
		e->impl = AZ_IMPL_FROM_TYPE (AZ_TYPE_DOUBLE);
		*((double *) e->bytes) = (double) tag * 1.5;
		break;
	case 3: /* complex double (one data slot) */
		e->impl = AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE);
		((AZComplexDouble *) e->bytes)->r = (double) tag;
		((AZComplexDouble *) e->bytes)->i = (double) (tag ^ 0xffff);
		break;
	case 4: { /* string (reference block) */
		uint8_t buf[32];
		snprintf ((char *) buf, 32, "s%08x", tag);
		e->impl = AZ_IMPL_FROM_TYPE (AZ_TYPE_STRING);
		e->ptr = az_string_new (buf);
		e->owned = (AZString *) e->ptr;
		break; }
	case 10: /* none (untyped null) */
		e->impl = NULL;
		break;
	default: { /* final struct of 12..48 bytes (1..3 data slots) */
		unsigned int s = kind - 5;
		e->impl = &struct_klasses[s]->impl;
		memset (e->bytes, (int) (tag & 0xff), struct_sizes[s]);
		break; }
	}
}

static void
push_entry (AZOStack *stack, const ModelEntry *e)
{
	if (!e->impl) {
		azo_stack_push_value (stack, NULL, NULL);
	} else if (AZ_IMPL_IS_BLOCK (e->impl)) {
		azo_stack_push_instance (stack, e->impl, e->ptr);
	} else {
		azo_stack_push_value (stack, e->impl, (const AZValue *) e->bytes);
	}
}

/* Verify the whole stack against the shadow model plus the global invariants */
static void
verify_stack (AZOStack *stack, unsigned int n)
{
	TEST_ASSERT_EQUAL_UINT (n, stack->length);
	for (unsigned int i = 0; i < n; i++) {
		TEST_ASSERT_EQUAL_PTR (model[i].impl, stack->elements[i].impl);
		if (!model[i].impl) continue;
		/* Boxed values are never stored on the stack */
		TEST_ASSERT (model[i].impl != &AZBoxedValueKlass.klass.impl);
		AZClass *klass = AZ_CLASS_FROM_IMPL (model[i].impl);
		if (AZ_CLASS_IS_BLOCK (klass)) {
			TEST_ASSERT_EQUAL_PTR (model[i].ptr, azo_stack_instance (stack, i));
		} else {
			TEST_ASSERT_EQUAL_MEMORY (model[i].bytes, azo_stack_instance (stack, i), klass->instance_size);
			if (klass->instance_size > 8) {
				/* Large value data slots remain below the data high-water mark */
				unsigned int blocks = (klass->instance_size + 15) / 16;
				TEST_ASSERT (stack->elements[i].val_idx + blocks <= stack->data_pos);
			}
		}
	}
}

/* Push a boxed struct value (the stack has to unbox it) */
static void
test_boxed_push (AZOStack *stack)
{
	uint8_t content[64];
	memset (content, 0x5a, 48);
	unsigned int base = stack->data_pos;
	AZBoxedValue *box = az_boxed_value_new_from_val (struct_klasses[4], (const AZValue *) content);
	AZValue val;
	val.block = box;
	/* Copy: the content lands on the stack, the box stays ours */
	azo_stack_push_value (stack, &AZBoxedValueKlass.klass.impl, &val);
	TEST_ASSERT (azo_stack_impl (stack, stack->length - 1) == &struct_klasses[4]->impl);
	TEST_ASSERT_EQUAL_MEMORY (content, azo_stack_instance (stack, stack->length - 1), 48);
	az_boxed_value_unref (box);
	/* Transfer: the content is moved out, the box released */
	box = az_boxed_value_new_from_val (struct_klasses[4], (const AZValue *) content);
	val.block = box;
	azo_stack_push_value_transfer (stack, &AZBoxedValueKlass.klass.impl, &val);
	TEST_ASSERT (azo_stack_impl (stack, stack->length - 1) == &struct_klasses[4]->impl);
	TEST_ASSERT_EQUAL_MEMORY (content, azo_stack_instance (stack, stack->length - 1), 48);
	azo_stack_pop (stack, 2);
	TEST_ASSERT_EQUAL_UINT (base, stack->data_pos);
}

static void
test_contraction (AZOStack *stack)
{
	uint8_t buf[64];
	AZComplexDouble cd = { 1.5, -2.5 };
	/* Pop contracts freed top slots */
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), (const AZValue *) &cd);
	TEST_ASSERT_EQUAL_UINT (1, stack->data_pos);
	memset (buf, 0x11, 32);
	azo_stack_push_value (stack, &struct_klasses[3]->impl, (const AZValue *) buf);
	TEST_ASSERT_EQUAL_UINT (3, stack->data_pos);
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32), (const AZValue *) buf);
	TEST_ASSERT_EQUAL_UINT (3, stack->data_pos);
	azo_stack_pop (stack, 1);
	TEST_ASSERT_EQUAL_UINT (3, stack->data_pos);
	/* The freed struct slots touch the high-water mark and are contracted */
	azo_stack_pop (stack, 1);
	TEST_ASSERT_EQUAL_UINT (1, stack->data_pos);
	/* The live complex double below is not contracted */
	TEST_ASSERT_EQUAL_MEMORY (&cd, stack->data + 0, 16);
	azo_stack_pop (stack, 1);
	TEST_ASSERT_EQUAL_UINT (0, stack->data_pos);
	TEST_ASSERT_EQUAL_UINT (0, stack->length);

	/* Remove from the middle leaves a hole, remove from the top contracts */
	memset (buf, 0x22, 16);
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), (const AZValue *) buf);
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32), (const AZValue *) buf);
	memset (buf, 0x33, 16);
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), (const AZValue *) buf);
	TEST_ASSERT_EQUAL_UINT (2, stack->data_pos);
	/* Remove the middle inline value: no data effect */
	azo_stack_remove (stack, 1, 1);
	TEST_ASSERT_EQUAL_UINT (2, stack->data_pos);
	TEST_ASSERT_EQUAL_UINT (2, stack->length);
	/* Remove the first (middle) complex double: a hole remains below the mark */
	azo_stack_remove (stack, 0, 1);
	TEST_ASSERT_EQUAL_UINT (2, stack->data_pos);
	TEST_ASSERT_EQUAL_UINT (1, stack->length);
	/* The surviving value keeps its slot and content */
	memset (buf, 0x33, 16);
	TEST_ASSERT_EQUAL_MEMORY (buf, azo_stack_instance (stack, 0), 16);
	/* Popping it contracts to the hole above (the hole itself is reclaimed at rewind) */
	azo_stack_pop (stack, 1);
	TEST_ASSERT_EQUAL_UINT (1, stack->data_pos);
	TEST_ASSERT_EQUAL_UINT (0, stack->length);

	/* Remove of the topmost elements is a pop (contracts); the hole left by the
	 * middle remove above keeps the mark at 1 (holes below the mark are
	 * reclaimed at frame rewind) */
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), (const AZValue *) &cd);
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32), (const AZValue *) buf);
	azo_stack_remove (stack, 0, 2);
	TEST_ASSERT_EQUAL_UINT (1, stack->data_pos);
	TEST_ASSERT_EQUAL_UINT (0, stack->length);
}

static void
test_store_transfer_reuse (AZOStack *stack)
{
	AZComplexDouble cd = { 3.0, 4.0 };
	AZComplexDouble cd2 = { 5.0, 6.0 };
	int32_t i32 = 42;
	unsigned int base = stack->data_pos;
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), (const AZValue *) &cd);
	TEST_ASSERT_EQUAL_UINT (base + 1, stack->data_pos);
	/* Same-size store reuses the slot */
	azo_stack_store_transfer (stack, 0, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), &cd2);
	TEST_ASSERT_EQUAL_UINT (base + 1, stack->data_pos);
	TEST_ASSERT_EQUAL_MEMORY (&cd2, stack->data + base, 16);
	/* Shrinking store goes inline (the slot becomes a hole, mark unchanged) */
	azo_stack_store_transfer (stack, 0, AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32), &i32);
	TEST_ASSERT_EQUAL_UINT (base + 1, stack->data_pos);
	TEST_ASSERT_EQUAL_INT32 (42, stack->elements[0].int32_v);
	/* Growing store appends a fresh slot */
	azo_stack_store_transfer (stack, 0, AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE), &cd);
	TEST_ASSERT_EQUAL_UINT (base + 2, stack->data_pos);
	TEST_ASSERT_EQUAL_MEMORY (&cd, stack->data + base + 1, 16);
	azo_stack_pop (stack, 1);
	TEST_ASSERT_EQUAL_UINT (0, stack->length);
}

static void
test_convert (AZOStack *stack)
{
	int32_t i32 = 42;
	unsigned int base = stack->data_pos;
	azo_stack_push_value (stack, AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32), (const AZValue *) &i32);
	TEST_ASSERT (azo_stack_convert (stack, 0, AZ_TYPE_COMPLEX_DOUBLE));
	TEST_ASSERT (azo_stack_impl (stack, 0) == AZ_IMPL_FROM_TYPE (AZ_TYPE_COMPLEX_DOUBLE));
	AZComplexDouble *cd = (AZComplexDouble *) azo_stack_instance (stack, 0);
	TEST_ASSERT (cd->r == 42.0 && cd->i == 0.0);
	TEST_ASSERT_EQUAL_UINT (base + 1, stack->data_pos);
	/* Back to a small value: inline again, slot left as a hole */
	TEST_ASSERT (azo_stack_convert (stack, 0, AZ_TYPE_INT32));
	TEST_ASSERT (azo_stack_impl (stack, 0) == AZ_IMPL_FROM_TYPE (AZ_TYPE_INT32));
	TEST_ASSERT_EQUAL_INT32 (42, stack->elements[0].int32_v);
	azo_stack_pop (stack, 1);
}

void
test_stack (void)
{
	az_init ();
	rng_state = 12345;
	register_struct_types ();
	AZOStack stack;
	az_instance_init_by_type (&stack, AZO_TYPE_STACK);

	/* Explicit scenarios */
	test_contraction (&stack);
	test_store_transfer_reuse (&stack);
	test_boxed_push (&stack);
	test_convert (&stack);
	verify_stack (&stack, 0);

	/* Randomized stress: ops mirrored by the shadow model */
	unsigned int len = 0;
	for (unsigned int op_count = 0; op_count < 20000; op_count++) {
		unsigned int op = rnd () % 9;
		if ((op <= 3) && (len < 400)) {
			/* push */
			unsigned int kind = rnd () % 11;
			model_fill (&model[len], kind, (uint32_t) op_count);
			push_entry (&stack, &model[len]);
			len += 1;
		} else if ((op == 4) && len) {
			/* pop */
			unsigned int n = 1 + rnd () % 3;
			if (n > len) n = len;
			model_drop (len - n, n);
			len -= n;
			azo_stack_pop (&stack, n);
		} else if ((op == 5) && len) {
			/* remove */
			unsigned int first = rnd () % len;
			unsigned int n = 1 + rnd () % 4;
			if ((first + n) > len) n = len - first;
			model_remove (first, n, &len);
			azo_stack_remove (&stack, first, n);
		} else if ((op == 6) && (len >= 2)) {
			/* exchange */
			unsigned int pos = rnd () % (len - 1);
			model_exchange (pos, &len);
			azo_stack_exchange (&stack, pos);
		} else if ((op == 7) && (len >= 2)) {
			/* roll */
			unsigned int pos = rnd () % (len - 1);
			model_roll (pos, &len);
			azo_stack_roll (&stack, pos);
		} else if ((op == 8) && len && (len < 400)) {
			/* duplicate */
			unsigned int pos = rnd () % len;
			model[len] = model[pos];
			/* The duplicate is a second stack reference, not a model-owned one */
			model[len].owned = NULL;
			azo_stack_duplicate (&stack, pos);
			len += 1;
		}
		verify_stack (&stack, len);
	}

	/* Tear down: the stack releases its references */
	model_drop (0, len);
	len = 0;
	az_instance_finalize_by_type (&stack, AZO_TYPE_STACK);
}
