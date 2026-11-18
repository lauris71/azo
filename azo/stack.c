#define __AZO_STACK_CPP__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2018
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <arikkei/arikkei-strlib.h>

#include <az/boxed-value.h>
#include <az/extend.h>

#include <azo/stack.h>

static void stack_class_init (AZOStackClass *klass);
static void stack_init (AZOStackClass *klass, AZOStack *stack);
static void stack_finalize (AZOStackClass *klass, AZOStack *stack);

static void stack_ensure_size (AZOStack *stack, unsigned int n_entries, unsigned int n_data);
/* Clear element, returning the number of freed data slots (its val_idx remains valid) */
unsigned int azo_stack_clear (AZOStack *stack, unsigned int pos);
static unsigned int stack_type = 0;
static AZOStackClass *stack_class = NULL;

unsigned int
azo_stack_get_type (void)
{
	if (!stack_type) {
		az_register_type (&stack_type, (const unsigned char *) "AZOStack", AZ_TYPE_BLOCK, sizeof (AZOStackClass), sizeof (AZOStack), AZ_FLAG_ZERO_MEMORY, 0, 0,
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
	stack->size = 256;
	stack->elements = (AZStoreElement *) malloc (stack->size * sizeof (AZStoreElement));
	stack->data_size = 256;
	stack->data = (AZValue *) malloc (stack->data_size * sizeof (AZValue));
}

static void
stack_finalize (AZOStackClass *klass, AZOStack *stack)
{
	for (unsigned int i = 0; i < stack->length; i++) {
		azo_stack_clear (stack, i);
	}
	free (stack->elements);
	free (stack->data);
}

static void
stack_ensure_size (AZOStack *stack, unsigned int n_entries, unsigned int n_data)
{
	if ((stack->length + n_entries) > stack->size) {
		stack->size = stack->size << 1;
		if ((stack->length + n_entries) > stack->size) stack->size = stack->length + n_entries;
		stack->elements = (AZStoreElement *) realloc (stack->elements, stack->size * sizeof (AZStoreElement));
	}
	if ((stack->data_pos + n_data) > stack->data_size) {
		stack->data_size = stack->data_size << 1;
		if ((stack->data_pos + n_data) > stack->data_size) stack->data_size = stack->data_pos + n_data;
		/* Data slots are referenced by index so no pointer fixup is needed */
		stack->data = (AZValue *) realloc (stack->data, stack->data_size * sizeof (AZValue));
	}
}

/* Clear element, returning the number of freed data slots (its val_idx remains valid) */
unsigned int
azo_stack_clear (AZOStack *stack, unsigned int idx)
{
	AZStoreElement *el = &stack->elements[idx];
	if (!el->impl) return 0;
	AZClass *klass = AZ_CLASS_FROM_IMPL (el->impl);
	if (AZ_IMPL_IS_REFERENCE (el->impl)) {
		/* fixme: az_value_store_clear_at does not guard NULL references */
		if (el->reference) az_reference_unref ((AZReferenceClass *) el->impl, el->reference);
		el->impl = NULL;
		return 0;
	}
	el->impl = NULL;
	if (AZ_CLASS_IS_BLOCK (klass) || (klass->instance_size <= 8)) return 0;
	return (klass->instance_size + 15) / 16;
}

/* Unbox boxed values: the stack never holds boxed values (the content is always a
 * plain value and stored as such). Boxed interfaces are lifecycle guards and kept */
static const AZImplementation *
stack_unbox (const AZImplementation *impl, void **inst)
{
	if (impl == &AZBoxedValueKlass.klass.impl) {
		AZBoxedValue *boxed = (AZBoxedValue *) *inst;
		*inst = &boxed->val;
		return &boxed->klass->impl;
	}
	return impl;
}

/* Set element, copying the instance (references are ref'd, large values appended at data_pos)
 * The caller has to ensure capacity (stack_ensure_size) beforehand */

static void
stack_set (AZOStack *stack, unsigned int idx, const AZImplementation *impl, void *inst)
{
	AZStoreElement *el = &stack->elements[idx];
	el->impl = impl;
	if (impl) {
		AZClass *klass = AZ_CLASS_FROM_IMPL (impl);
		if (AZ_CLASS_IS_BLOCK (klass)) {
			el->block = inst;
			/* fixme: az_value_store_set refs unconditionally, crashing on typed nulls */
			if (inst && AZ_CLASS_IS_REFERENCE (klass)) az_reference_ref ((AZReference *) inst);
		} else if (klass->instance_size <= 8) {
			memcpy (el->val, inst, klass->instance_size);
		} else {
			el->val_idx = stack->data_pos;
			memcpy (stack->data + stack->data_pos, inst, klass->instance_size);
			stack->data_pos += (klass->instance_size + 15) / 16;
		}
	}
}

/* Set element, transferring ownership of the instance (large values appended at data_pos)
 * The caller has to ensure capacity (stack_ensure_size) beforehand */
static void
stack_transfer_in (AZOStack *stack, unsigned int idx, const AZImplementation *impl, void *inst)
{
	AZStoreElement *el = &stack->elements[idx];
	/* Boxed values are unboxed and the box released (transfer) */
	AZBoxedValue *box = NULL;
	if (impl && (AZ_CLASS_FROM_IMPL (impl) == (AZClass *) &AZBoxedValueKlass)) {
		box = (AZBoxedValue *) inst;
		impl = &box->klass->impl;
		inst = &box->val;
	}
	el->impl = impl;
	if (impl) {
		AZClass *klass = AZ_CLASS_FROM_IMPL (impl);
		if (AZ_CLASS_IS_BLOCK (klass)) {
			el->block = inst;
		} else if (klass->instance_size <= 8) {
			memcpy (el->val, inst, klass->instance_size);
		} else {
			el->val_idx = stack->data_pos;
			memcpy (stack->data + stack->data_pos, inst, klass->instance_size);
			stack->data_pos += (klass->instance_size + 15) / 16;
		}
	}
}

void
azo_stack_pop (AZOStack *stack, unsigned int n_values)
{
	arikkei_return_if_fail (n_values <= stack->length);
	for (unsigned int i = 0; i < n_values; i++) {
		stack->length -= 1;
		/* Only freed slots are contracted (the val_idx of a cleared element remains valid) */
		unsigned int freed = azo_stack_clear (stack, stack->length);
		if (freed && ((stack->elements[stack->length].val_idx + freed) == stack->data_pos)) {
			stack->data_pos = stack->elements[stack->length].val_idx;
		}
	}
}

void
azo_stack_remove (AZOStack *stack, unsigned int first, unsigned int n_values)
{
	arikkei_return_if_fail ((first + n_values) <= stack->length);
	if ((first + n_values) == stack->length) {
		/* Removing from the top is a pop (contracts freed data slots) */
		azo_stack_pop (stack, n_values);
		return;
	}
	for (unsigned int i = 0; i < n_values; i++) {
		azo_stack_clear (stack, first + i);
	}
	memmove (&stack->elements[first], &stack->elements[first + n_values], (stack->length - first - n_values) * sizeof (AZStoreElement));
	stack->length -= n_values;
	/* The data area is not compacted (transient holes are reclaimed at frame rewind) */
}

void
azo_stack_push_value_default (AZOStack *stack, const AZImplementation *impl)
{
	unsigned int blocks = az_value_store_block_size (impl);
	stack_ensure_size (stack, 1, blocks);
	AZStoreElement *el = &stack->elements[stack->length];
	el->impl = impl;
	if (impl) {
		AZClass *klass = AZ_CLASS_FROM_IMPL (impl);
		if (AZ_CLASS_IS_BLOCK (klass)) {
			el->block = NULL;
		} else if (klass->instance_size <= 8) {
			az_value_init (impl, (AZValue *) el->val);
		} else {
			el->val_idx = stack->data_pos;
			az_value_init (impl, stack->data + stack->data_pos);
			stack->data_pos += blocks;
		}
	}
	stack->length += 1;
}

void
azo_stack_push_value (AZOStack *stack, const AZImplementation *impl, const void *value)
{
	void *n_inst;
	const AZImplementation *n_impl = az_value_get_inst_autobox(impl, (const AZValue *) value, &n_inst);
	stack_ensure_size (stack, 1, az_value_store_block_size (n_impl));
	stack_set (stack, stack->length, n_impl, n_inst);
	stack->length += 1;
}

void
azo_stack_push_value_transfer (AZOStack *stack, const AZImplementation *impl, void *value)
{
	void *n_inst;
	const AZImplementation *n_impl = az_value_get_inst_autobox(impl, (const AZValue *) value, &n_inst);
	stack_ensure_size (stack, 1, az_value_store_block_size (n_impl));
	stack_set (stack, stack->length, n_impl, n_inst);
	az_value_clear(impl, (AZValue *) value);
	stack->length += 1;
}

void
azo_stack_push_instance (AZOStack *stack, const AZImplementation *impl, void *inst)
{
	impl = stack_unbox (impl, &inst);
	stack_ensure_size (stack, 1, az_value_store_block_size (impl));
	stack_set (stack, stack->length, impl, inst);
	stack->length += 1;
}

void
azo_stack_push_elements (AZOStack *stack, unsigned int n_elements, const AZStoreElement elements[], AZValue data[])
{
	for (unsigned int i = 0; i < n_elements; i++) {
		const AZImplementation *impl = elements[i].impl;
		void *inst = az_value_store_instance_at ((AZStoreElement *) elements, i, data);
		impl = stack_unbox (impl, &inst);
		stack_ensure_size (stack, 1, az_value_store_block_size (impl));
		stack_set (stack, stack->length, impl, inst);
		stack->length += 1;
	}
}

void
azo_stack_duplicate (AZOStack *stack, unsigned int pos)
{
	azo_stack_push_value (stack, stack->elements[pos].impl, azo_stack_value (stack, pos));
}

void
azo_stack_exchange (AZOStack *stack, unsigned int pos)
{
	arikkei_return_if_fail (pos < stack->length - 1);
	/* Data slots are position-independent so swapping the elements suffices */
	AZStoreElement el = stack->elements[pos];
	stack->elements[pos] = stack->elements[stack->length - 1];
	stack->elements[stack->length - 1] = el;
}

void
azo_stack_roll (AZOStack *stack, unsigned int pos)
{
	arikkei_return_if_fail (pos < stack->length - 1);
	/* Move-insert the top element to pos (data slots are position-independent) */
	AZStoreElement el = stack->elements[stack->length - 1];
	memmove (&stack->elements[pos + 1], &stack->elements[pos], (stack->length - 1 - pos) * sizeof (AZStoreElement));
	stack->elements[pos] = el;
}

void
azo_stack_store_transfer (AZOStack *stack, unsigned int pos, const AZImplementation *impl, void *value)
{
	AZStoreElement *el = &stack->elements[pos];
	/* Boxed values are unboxed and the box released (transfer) */
	AZBoxedValue *box = NULL;
	if (impl && (AZ_CLASS_FROM_IMPL (impl) == (AZClass *) &AZBoxedValueKlass)) {
		box = *((AZBoxedValue **) value);
		impl = &box->klass->impl;
		value = &box->val;
	}
	unsigned int new_blocks = az_value_store_block_size (impl);
	/* Release the old reference (plain values need no destruction) */
	if (el->impl && AZ_IMPL_IS_REFERENCE (el->impl) && el->reference) {
		az_reference_unref ((AZReferenceClass *) el->impl, el->reference);
	}
	if (new_blocks) {
		unsigned int old_blocks = (el->impl && !AZ_IMPL_IS_BLOCK (el->impl)) ? az_value_store_block_size_at (stack->elements, pos) : 0;
		if (old_blocks >= new_blocks) {
			/* Reuse the existing data slot */
			memcpy (stack->data + el->val_idx, value, AZ_CLASS_FROM_IMPL (impl)->instance_size);
		} else {
			/* Append a fresh slot (the old one is reclaimed at frame rewind) */
			stack_ensure_size (stack, 0, new_blocks);
			el->val_idx = stack->data_pos;
			memcpy (stack->data + stack->data_pos, value, AZ_CLASS_FROM_IMPL (impl)->instance_size);
			stack->data_pos += new_blocks;
		}
	} else if (impl) {
		AZClass *klass = AZ_CLASS_FROM_IMPL (impl);
		if (AZ_CLASS_IS_BLOCK (klass)) {
			el->block = *((void **) value);
		} else {
			memcpy (el->val, value, klass->instance_size);
		}
	}
	el->impl = impl;
	if (box) az_boxed_value_unref (box);
}

unsigned int
azo_stack_convert (AZOStack *stack, unsigned int pos, unsigned int to_type)
{
	const AZImplementation *impl = stack->elements[pos].impl;
	unsigned int src_type = impl ? AZ_IMPL_TYPE (impl) : 0;
	/* Primitive identity: the conversion is a no-op (impl stays concrete) */
	if (impl && AZ_TYPE_IS_PRIMITIVE (src_type) && ((to_type == src_type) || (to_type == AZ_TYPE_ANY))) return 1;
	if (!impl && (to_type == AZ_TYPE_ANY)) return 1;
	/* Convert into a scratch and store back; the only storage-allocating
	 * result is complex double (all other values fit inline, blocks are
	 * pointers) so the slot data storage is never grown in place */
	AZValue scratch;
	const AZImplementation *new_impl = impl;
	if (az_value_convert_autobox (&new_impl, &scratch, 16, impl, azo_stack_value (stack, pos), to_type, AZ_CONVERT_CONDITIONAL) == AZ_CONVERSION_FAILED) {
		return 0;
	}
	azo_stack_store_transfer (stack, pos, new_impl, &scratch);
	return 1;
}

unsigned int
azo_stack_print_element(AZOStack *stack, unsigned int pos, uint8_t *d, unsigned int d_len)
{
	uint8_t buf[1024];
	const char *name = "NONE";
	unsigned int type = azo_stack_type (stack, pos);
	unsigned int size = 0;
	if (type) {
		AZClass *klass = AZ_CLASS_FROM_TYPE (type);
		size = AZ_CLASS_IS_BLOCK (klass) ? 0 : klass->instance_size;
		name = (const char *) klass->name;
		az_instance_to_string (AZ_IMPL_FROM_TYPE(type), azo_stack_instance (stack, pos), buf, 1024);
	} else {
		buf[0] = 0;
	}
	unsigned int len = snprintf((char *) d, d_len, "%u: %2u\t%u %s %s", pos, size, type, name, buf);
	if (d_len && (len >= d_len)) d[d_len - 1] = 0;
	return len;
}

void
azo_stack_print_contents (AZOStack *stack, unsigned int start, unsigned int end, FILE *ofs)
{
	unsigned int i;
	for (i = start; i < end; i++) {
		unsigned char b[1024];
		const unsigned char *name;
		unsigned int pos = end - 1 - (i - start);
		unsigned int type = azo_stack_type (stack, pos);
		unsigned int size = 0;
		if (type) {
			AZClass *klass = AZ_CLASS_FROM_TYPE (type);
			size = AZ_CLASS_IS_BLOCK (klass) ? 0 : klass->instance_size;
			name = klass->name;
			az_instance_to_string (AZ_IMPL_FROM_TYPE(type), azo_stack_instance (stack, pos), b, 1024);
		} else {
			name = (const unsigned char *) "NONE";
			b[0] = 0;
		}
		fprintf (stderr, "%u: %2u\t%u %s %s\n", pos, size, type, name, b);
	}
}
