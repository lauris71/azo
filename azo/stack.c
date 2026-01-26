#define __AZO_STACK_CPP__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2018
*/

#define COMPACT

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <az/extend.h>

#include <azo/stack.h>

static void stack_class_init (AZOStackClass *klass);
static void stack_init (AZOStackClass *klass, AZOStack *stack);
static void stack_finalize (AZOStackClass *klass, AZOStack *stack);

static void stack_ensure_size (AZOStack *stack, unsigned int n_entries, unsigned int data_size);

#define STACK_MIN_SIZE 16
#define STACK_MIN_MASK (STACK_MIN_SIZE - 1)
#define STACK_IMPL_VALUE_SIZE(i) STACK_VALUE_SIZE(AZ_CLASS_FROM_IMPL(i))
#define STACK_ELEMENT_SIZE(s,p) (unsigned int) (stack->values[(p) + 1].ptr - stack->values[p].ptr)

static inline unsigned int
STACK_VALUE_SIZE(AZClass *klass)
{
	return (klass && AZ_CLASS_VALUE_SIZE(klass)) ? (AZ_CLASS_ELEMENT_SIZE(klass) + STACK_MIN_MASK) & ~STACK_MIN_MASK : STACK_MIN_SIZE;

}

static unsigned int stack_type = 0;
static AZOStackClass *stack_class = NULL;

unsigned int
azo_stack_get_type (void)
{
	if (!stack_type) {
		az_register_type (&stack_type, (const unsigned char *) "AZOStack", AZ_TYPE_BLOCK, sizeof (AZOStackClass), sizeof (AZOStack), AZ_FLAG_ZERO_MEMORY,
			(void (*) (AZClass *)) stack_class_init,
			(void (*) (const AZImplementation *, void *)) stack_init,
			(void (*) (const AZImplementation *, void *)) stack_finalize);
	}
	return stack_type;
}

static void
stack_class_init (AZOStackClass *klass)
{
}

static void
stack_init (AZOStackClass *klass, AZOStack *stack)
{
	stack->data_size = 4096;
	stack->data = (unsigned char *) malloc (stack->data_size);
	stack->size = 256;
	stack->impls = (const AZImplementation **) malloc (stack->size * sizeof (AZImplementation *));
	stack->values = (AZStackEntry *) malloc (stack->size * sizeof (AZStackEntry));
	stack->values[0].ptr = stack->data;
}

static void
stack_finalize (AZOStackClass *klass, AZOStack *stack)
{
	unsigned int i;
	for (i = 0; i < stack->length; i++) {
		if (azo_stack_impl (stack, i)) az_value_clear (azo_stack_impl (stack, i), azo_stack_value (stack, i));
	}
	free ((void *) stack->impls);
	free (stack->values);
	free (stack->data);
}

static void
stack_ensure_size (AZOStack *stack, unsigned int n_entries, unsigned int data_size)
{
	assert ((data_size & (STACK_MIN_SIZE - 1)) == 0);
	if ((stack->length + n_entries + 1) >= stack->size) {
		stack->size = stack->size << 1;
		if ((stack->length + n_entries + 1) >= stack->size) stack->size = stack->length + n_entries + 1;
		stack->impls = (const AZImplementation **) realloc ((void *) stack->impls, stack->size * sizeof (AZImplementation *));
		stack->values = (AZStackEntry *) realloc (stack->values, stack->size * sizeof (AZStackEntry));
	}
	if ((stack->values[stack->length].ptr + data_size) >= (stack->data + stack->data_size)) {
		stack->data_size = stack->data_size << 1;
		if ((stack->values[stack->length].ptr + data_size) >= (stack->data + stack->data_size)) stack->data_size = stack->data_size + data_size;
		stack->data = (unsigned char *) realloc (stack->data, stack->data_size);
	}
}

static void
stack_ensure_element_size (AZOStack *stack, unsigned int pos, unsigned int size)
{
	if ((stack->values[pos + 1].ptr - stack->values[pos].ptr) < size) {
		unsigned int shift, i;
		shift = size - (unsigned int) (stack->values[pos + 1].ptr - stack->values[pos].ptr);
		stack_ensure_size (stack, 0, shift);
		memcpy (stack->values[pos + 1].ptr + shift, stack->values[pos + 1].ptr, stack->values[stack->length].ptr - stack->values[pos + 1].ptr);
		for (i = pos + 1; i <= stack->length; i++) stack->values[i].ptr += shift;
	}
}

void
azo_stack_pop (AZOStack *stack, unsigned int n_data)
{
	unsigned int i;
	arikkei_return_if_fail (n_data <= stack->length);
	for (i = 0; i < n_data; i++) {
		stack->length -= 1;
		az_value_clear (azo_stack_impl (stack, stack->length), azo_stack_value (stack, stack->length));
	}
}

void
azo_stack_remove (AZOStack *stack, unsigned int first, unsigned int n_data)
{
	unsigned int i;
	arikkei_return_if_fail ((first + n_data) <= stack->length);
	for (i = 0; i < n_data; i++) {
		az_value_clear (azo_stack_impl (stack, first + i), azo_stack_value (stack, first + i));
	}
	if ((first + n_data) < stack->length) {
		unsigned int n_tail = stack->length - (first + n_data);
		const unsigned char *start;
		if (first > 0) {
			start = (const unsigned char *) azo_stack_value(stack, first - 1) + STACK_IMPL_VALUE_SIZE(azo_stack_impl(stack, first - 1));
		} else {
			start = stack->data;
		}
		const unsigned char *mid = (const unsigned char *) azo_stack_value (stack, first + n_data);
		const unsigned char *end = (const unsigned char *) azo_stack_value (stack, stack->length);
		memcpy ((unsigned char *) start, mid, end - mid);
		for (i = 0; i <= n_tail; i++) {
			stack->impls[first + i] = stack->impls[first + n_data + i];
			stack->values[first + i].ptr = stack->values[first + n_data + i].ptr - (mid - start);
		}
	}
	stack->length -= n_data;
}

void
azo_stack_push_value_default (AZOStack *stack, const AZImplementation *impl)
{
	unsigned int size;
	stack->impls[stack->length] = impl;
	if (impl) {
		size = STACK_IMPL_VALUE_SIZE(impl);
		stack_ensure_size (stack, 1, size);
		az_value_init (impl, azo_stack_value (stack, stack->length));
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + size;
	} else {
		stack_ensure_size (stack, 1, STACK_MIN_SIZE);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + STACK_MIN_SIZE;
	}
	stack->length += 1;
}

void
azo_stack_push_value (AZOStack *stack, const AZImplementation *impl, const void *value)
{
	unsigned int size;
	stack->impls[stack->length] = impl;
	if (impl) {
		size = STACK_IMPL_VALUE_SIZE(impl);
		stack_ensure_size (stack, 1, size);
		az_value_copy (impl, azo_stack_value (stack, stack->length), value);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + size;
	} else {
		stack_ensure_size (stack, 1, STACK_MIN_SIZE);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + STACK_MIN_SIZE;
	}
	stack->length += 1;
}

void
azo_stack_push_value_transfer (AZOStack *stack, const AZImplementation *impl, void *value)
{
	unsigned int size;
	stack->impls[stack->length] = impl;
	if (impl) {
		size = STACK_VALUE_SIZE(AZ_CLASS_FROM_IMPL(impl));
		stack_ensure_size (stack, 1, size);
		az_value_transfer (impl, azo_stack_value (stack, stack->length), value);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + size;
	} else {
		stack_ensure_size (stack, 1, STACK_MIN_SIZE);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + STACK_MIN_SIZE;
	}
	stack->length += 1;
}

void
azo_stack_push_instance (AZOStack *stack, const AZImplementation *impl, void *inst)
{
	unsigned int size;
	stack->impls[stack->length] = impl;
	if (impl) {
		size = STACK_VALUE_SIZE(AZ_CLASS_FROM_IMPL(impl));
		stack_ensure_size (stack, 1, size);
		az_value_set_from_inst (impl, azo_stack_value (stack, stack->length), inst);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + size;
	} else {
		stack_ensure_size (stack, 1, STACK_MIN_SIZE);
		stack->values[stack->length + 1].ptr = stack->values[stack->length].ptr + STACK_MIN_SIZE;
	}
	stack->length += 1;
}

void
azo_stack_duplicate (AZOStack *stack, unsigned int pos)
{
	azo_stack_push_value (stack, azo_stack_impl (stack, pos), azo_stack_value (stack, pos));
}

void
azo_stack_exchange (AZOStack *stack, unsigned int pos)
{
	unsigned int size_a, size_b;
	const AZImplementation *impl;
	arikkei_return_if_fail (pos < stack->length - 1);
	size_a = STACK_IMPL_VALUE_SIZE(azo_stack_impl(stack, pos));
	size_b = STACK_IMPL_VALUE_SIZE(azo_stack_impl(stack, stack->length - 1));
	if (size_a == size_b) {
		stack_ensure_size (stack, 1, size_a);
		memcpy (azo_stack_value (stack, stack->length), azo_stack_value (stack, pos), size_a);
		memcpy (azo_stack_value (stack, pos), azo_stack_value (stack, stack->length - 1), size_b);
		memcpy (azo_stack_value (stack, stack->length - 1), azo_stack_value (stack, stack->length), size_a);

		impl = stack->impls[pos];
		stack->impls[pos] = stack->impls[stack->length - 1];
		stack->impls[stack->length - 1] = impl;
	} else if (size_a > size_b) {
		unsigned int i;
		unsigned int shift = size_a - size_b;
		stack_ensure_size (stack, 0, size_a);
		/* Copy A to end of stack  */
		memcpy (stack->values[stack->length].ptr, stack->values[pos].ptr, size_a);
		/* Copy B to A-s position */
		memcpy (stack->values[pos].ptr, stack->values[stack->length - 1].ptr, size_b);
		/* Shift remainder left  */
		memcpy (stack->values[pos + 1].ptr - shift, stack->values[pos + 1].ptr, stack->values[stack->length - 1].ptr - stack->values[pos + 1].ptr);
		/* Copy B to len - 1 */
		memcpy (stack->values[stack->length - 1].ptr - shift, stack->values[stack->length].ptr, size_a);
		/* Shift entries */
		for (i = pos + 1; i <= stack->length; i++) stack->values[i].ptr -= shift;

		impl = stack->impls[pos];
		stack->impls[pos] = stack->impls[stack->length - 1];
		stack->impls[stack->length - 1] = impl;
	} else {
		unsigned int i;
		unsigned int shift = size_b - size_a;
		/* Ensure we have shift + size_a free space */
		stack_ensure_size (stack, 0, size_b);
		/* 1: Shift [a+1 - end) right */
		memcpy (stack->values[pos + 1].ptr + shift, stack->values[pos + 1].ptr, stack->values[stack->length].ptr - stack->values[pos + 1].ptr);
		/* 2: Copy A to end of shifted stack  */
		memcpy (stack->values[stack->length].ptr + shift, stack->values[pos].ptr, size_a);
		/* 3: Copy shifted B to A */
		memcpy (stack->values[pos].ptr, stack->values[stack->length - 1].ptr + shift, size_b);
		/* Copy A to len - 1 */
		memcpy (stack->values[stack->length - 1].ptr + shift, stack->values[stack->length].ptr + shift, size_a);

		/* Shift entries */
		for (i = pos + 1; i <= stack->length; i++) stack->values[i].ptr += shift;

		impl = stack->impls[pos];
		stack->impls[pos] = stack->impls[stack->length - 1];
		stack->impls[stack->length - 1] = impl;
	}
}

unsigned int
azo_stack_convert (AZOStack *stack, unsigned int pos, unsigned int to_type)
{
	AZClass *klass;
	arikkei_return_val_if_fail (pos < stack->length, 0);
	klass = az_type_get_class (to_type);
	stack_ensure_element_size (stack, pos, az_class_value_size(klass));
	if (!az_value_convert_in_place (&stack->impls[pos], (AZValue *) stack->values[pos].ptr, to_type)) return 0;
	return 1;
}

void
azo_stack_print_contents (AZOStack *stack, unsigned int start, unsigned int end, FILE *ofs)
{
	unsigned int i;
	for (i = start; i < end; i++) {
		unsigned char b[1024];
		const unsigned char *name;
		unsigned int pos, type;
		pos = end - 1 - (i - start);
		type = azo_stack_type (stack, pos);
		if (type) {
			unsigned int len;
			name = AZ_CLASS_FROM_TYPE(type)->name;
			az_instance_to_string (AZ_IMPL_FROM_TYPE(type), azo_stack_instance (stack, pos), b, 1024);
		} else {
			name = (const unsigned char *) "NONE";
			b[0] = 0;
		}
		fprintf (stderr, "%u: %2u\t%u %s %s\n", pos, (unsigned int) (stack->values[pos + 1].ptr - stack->values[pos].ptr), type, name, b);
	}
}

