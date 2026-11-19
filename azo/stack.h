#ifndef __AZO_STACK_H__
#define __AZO_STACK_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2018
*/

typedef struct _AZOStack AZOStack;
typedef struct _AZOStackClass AZOStackClass;

#define AZO_TYPE_STACK azo_stack_get_type ()

#include <stdio.h>

#include <az/base.h>
#include <az/class.h>
#include <az/value.h>
#include <az/value-store.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The value stack: an array of AZStoreElements plus a data buffer of 16-byte
 * granularity (the az value store framework).
 *
 * Values up to 8 bytes are stored inline in the elements, larger values as
 * 16-byte unit indices into the data buffer, blocks as pointers. Data slots
 * are allocated lazily (append-only at the data high-water mark) and free
 * slots are only contracted from the top (pop/rewind) - element operations
 * (remove/exchange/roll) never move data, so transient holes may remain below
 * the high-water mark until the frame is rewound.
 *
 * Because data slots are referenced by index the buffers can be reallocated
 * without any pointer fixup.
 */
struct _AZOStack {
	unsigned int size;         /* element capacity */
	unsigned int length;       /* element count */
	AZStoreElement *elements;
	unsigned int data_size;    /* data buffer capacity in 16-byte units */
	unsigned int data_pos;     /* data high-water mark in 16-byte units */
	AZValue *data;             /* 16-byte aligned data buffer */
};

struct _AZOStackClass {
	AZClass klass;
};

unsigned int azo_stack_get_type (void);

/* Clear element, returning the number of freed data slots (its val_idx remains valid) */
unsigned int azo_stack_clear (AZOStack *stack, unsigned int pos);
/* Remove N topmost elements (contracting the data area) */
void azo_stack_pop (AZOStack *stack, unsigned int n_values);
/* Remove N elements, starting from given position */
void azo_stack_remove (AZOStack *stack, unsigned int first, unsigned int n_values);
static inline
void azo_stack_remove_bw (AZOStack *stack, unsigned int pos_from_last, unsigned int n_values)
{
	azo_stack_remove (stack, stack->length - 1 - pos_from_last, n_values);
}
/* It is legal to push NULL values to stack */
void azo_stack_push_value_default (AZOStack *stack, const AZImplementation *impl);
void azo_stack_push_value (AZOStack *stack, const AZImplementation *impl, const void *value);
void azo_stack_push_value_transfer (AZOStack *stack, const AZImplementation *impl, void *value);
void azo_stack_push_instance (AZOStack *stack, const AZImplementation *impl, void *inst);
/* Push n store elements (copying, references ref'd) */
void azo_stack_push_elements (AZOStack *stack, unsigned int n_elements, const AZStoreElement elements[], AZValue data[]);

static inline void
azo_stack_push_u32(AZOStack *stack, uint32_t val)
{
	azo_stack_push_instance(stack, &AZUint32Klass.impl, &val);
}

/* Duplicate element to the top of stack */
void azo_stack_duplicate (AZOStack *stack, unsigned int pos);

/* Exchange element with the top of stack */
void azo_stack_exchange (AZOStack *stack, unsigned int pos);
ARIKKEI_INLINE
void azo_stack_exchange_bw (AZOStack *stack, unsigned int pos_from_last)
{
	azo_stack_exchange (stack, stack->length - 1 - pos_from_last);
}

/* Move-insert the last element to given position */
void azo_stack_roll (AZOStack *stack, unsigned int pos);
ARIKKEI_INLINE
void azo_stack_roll_bw (AZOStack *stack, unsigned int pos_from_last)
{
	azo_stack_roll (stack, stack->length - 1 - pos_from_last);
}

/* Store value into an existing slot (transfer, reusing the slot's data storage when it fits) */
void azo_stack_store_transfer (AZOStack *stack, unsigned int pos, const AZImplementation *impl, void *value);
ARIKKEI_INLINE
void azo_stack_store_transfer_bw (AZOStack *stack, unsigned int pos_from_last, const AZImplementation *impl, void *value)
{
	azo_stack_store_transfer (stack, stack->length - 1 - pos_from_last, impl, value);
}

/* Change type of value */
unsigned int azo_stack_convert (AZOStack *stack, unsigned int pos, unsigned int to_type);
ARIKKEI_INLINE
unsigned int azo_stack_convert_bw (AZOStack *stack, unsigned int pos_from_last, unsigned int to_type)
{
	return azo_stack_convert (stack, stack->length - 1 - pos_from_last, to_type);
}

ARIKKEI_INLINE
unsigned int azo_stack_type (AZOStack *stack, unsigned int idx)
{
	return (stack->elements[idx].impl) ? AZ_IMPL_TYPE(stack->elements[idx].impl) : 0;
}

ARIKKEI_INLINE
unsigned int azo_stack_type_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_type (stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
const AZImplementation *azo_stack_impl (AZOStack *stack, unsigned int idx)
{
	return stack->elements[idx].impl;
}

ARIKKEI_INLINE
const AZImplementation *azo_stack_impl_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_impl(stack, stack->length - 1 - pos_from_last);
}

static inline AZValue *
azo_stack_value(AZOStack *stack, unsigned int idx)
{
	/* Uninitialized (NULL impl) elements have no value storage */
	return az_value_store_value_at(stack->elements, idx, stack->data);
}

static inline AZValue *
azo_stack_value_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_value (stack, stack->length - 1 - pos_from_last);
}

static inline AZValue *
azo_stack_primitive(AZOStack *stack, unsigned int idx)
{
	return azo_stack_value (stack, idx);
}

static inline AZValue *
azo_stack_primitive_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_value (stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
void *azo_stack_instance (AZOStack *stack, unsigned int idx)
{
	return az_value_store_instance_at(stack->elements, idx, stack->data);
}

ARIKKEI_INLINE
void *azo_stack_instance_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_instance (stack, stack->length - 1 - pos_from_last);
}

static inline unsigned int
azo_stack_boolean_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return stack->elements[stack->length - 1 - pos_from_last].boolean_v;
}

static inline int32_t
azo_stack_int32_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return stack->elements[stack->length - 1 - pos_from_last].int32_v;
}

static inline uint32_t
azo_stack_uint32_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return stack->elements[stack->length - 1 - pos_from_last].uint32_v;
}

static inline void *
azo_stack_pointer_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return stack->elements[stack->length - 1 - pos_from_last].pointer_v;
}

unsigned int azo_stack_print_element(AZOStack *stack, unsigned int pos, uint8_t *d, unsigned int d_len);
void azo_stack_print_contents (AZOStack *stack, unsigned int start, unsigned int end, FILE *ofs);

#ifdef __cplusplus
};
#endif

#endif
