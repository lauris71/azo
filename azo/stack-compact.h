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

#include <az/class.h>
#include <az/value.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convenience union for performing pointer arithmetic */

typedef union _AZStackEntry AZStackEntry;

union _AZStackEntry {
	AZValue *val;
	unsigned char *ptr;
};

struct _AZOStack {
	unsigned int size;
	unsigned int length;
	/* Entries have (length + 1) elements, the last pointing to the first free value */
	const AZImplementation *(*impls);
	AZStackEntry *values;
	unsigned int data_size;
	unsigned char *data;
};

struct _AZOStackClass {
	AZClass klass;
};

unsigned int azo_stack_get_type (void);

/* Remove N topmost elements */
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
/* Duplicate element to the top of stack */
void azo_stack_duplicate (AZOStack *stack, unsigned int pos);

/* Exchnage element with the top of stack */
void azo_stack_exchange (AZOStack *stack, unsigned int pos);
ARIKKEI_INLINE
void azo_stack_exchange_bw (AZOStack *stack, unsigned int pos_from_last)
{
	azo_stack_exchange (stack, stack->length - 1 - pos_from_last);
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
	return (stack->impls[idx]) ? AZ_IMPL_TYPE(stack->impls[idx]) : 0;
}

ARIKKEI_INLINE
unsigned int azo_stack_type_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_type (stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
const AZImplementation *azo_stack_impl (AZOStack *stack, unsigned int idx)
{
	return stack->impls[idx];
}

ARIKKEI_INLINE
const AZImplementation *azo_stack_impl_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_impl(stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
const AZImplementation **azo_stack_impls (AZOStack *stack, unsigned int idx)
{
	return &stack->impls[idx];
}

ARIKKEI_INLINE
const AZImplementation **azo_stack_impls_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_impls (stack, stack->length - 1 - pos_from_last);
}

static inline AZValue *
azo_stack_value(AZOStack *stack, unsigned int idx)
{
	return stack->values[idx].val;
}

static inline AZValue *
azo_stack_value_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_value (stack, stack->length - 1 - pos_from_last);
}

static inline AZValue *
azo_stack_primitive(AZOStack *stack, unsigned int idx)
{
	return stack->values[idx].val;
}

static inline AZValue *
azo_stack_primitive_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_value (stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
AZValue **azo_stack_values (AZOStack *stack, unsigned int idx)
{
	return &stack->values[idx].val;
}

ARIKKEI_INLINE
AZValue **azo_stack_values_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_values (stack, stack->length - 1 - pos_from_last);
}

ARIKKEI_INLINE
void *azo_stack_instance (AZOStack *stack, unsigned int idx)
{
	return (stack->impls[idx]) ? az_value_get_inst(stack->impls[idx], stack->values[idx].val) : NULL;
}

ARIKKEI_INLINE
void *azo_stack_instance_bw (AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_instance (stack, stack->length - 1 - pos_from_last);
}

static inline unsigned int
azo_stack_boolean_bw(AZOStack *stack, unsigned int pos_from_last)
{
	return azo_stack_primitive_bw(stack, pos_from_last)->boolean_v;
}

void azo_stack_print_contents (AZOStack *stack, unsigned int start, unsigned int end, FILE *ofs);

#ifdef __cplusplus
};
#endif

#endif
