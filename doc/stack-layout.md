# Stack layout variants:

## Prerequisites

For each type, stack has to store both implementation and instance. The implementation provides runtime type information
for the cases where the type is not known at compile time.
For non-interface types we can use 32-bit typecode - but getting the implementation/class from typecode involves mutex locking.
Also, the stack representation should be the same as argument representation for AZFunction invoke method to avoid copying arguments around.

Also - the vast majority of stack elements are small primitives and block pointers. Complex double is the only primitive that does not fit
into 8 bytes (and is not commonly used). Other commonly used large value types are 3 and 4-component float vectors (12-16 bytes) and matrices (36, 48 or 64 bytes)

## The current layout
 - array of `const AZImplementation *`
 - array of `AZValue *`
 - data block

Each stack entry takes it's `value_size` bytes (rounded up to 16-byte boundary) from the datablock. The array of values points to the corresponding location in the data block.
Whenever (value_size % 16) differs between elements, intra-stack operations (remove, roll) must shift the following entries to adjust the value size (but
shifting is very rare because most value types fit into 16 bytes)

One stack entry takes 32+ bytes (8 bytes impl pointer, 8 bytes val pointer, min 16 bytes data entry)

Drawbacks
 - for block types we need two pointer dereferences (value->data->instance)
 - 32 bytes per int is wasteful
 - determining whether a type is value or block from the implementation takes one pointer dereference

## An alternative

 - array of `const AZImplementation *`
 - array of `void *` (pointers to instances)
 - data block

Value-type entries would take their ìnstance_size` (the same as value-size), rounded to 16-byte boundary, from the datablock. Block-type entries do not take any room from the data block. Instance pointer points to:
 - for value types - the instance inside datablock
 - block types - directly to the instance in heap

One stack entry takes min 16 bytes (block types), 16 + value_size for value types

Pluses
 - this removes the double dereferences of block types.

Drawbacks
 - small values still take 32 bytes
 - still needs shifting if the value size differs (and shifting occurs often because block types now have datablock size 0)

Or we can make the minimum datablock allocation 16 bytes - then it is almost exactly as the current variant, only saves one dereference

## More complex alternative

 - array of `const AZImplementation *`
 - array of value/pointer unions
 - data block

The unions could contain:
 - a pointer to instance for block types
 - small value (up to 8 bytes) for value types
 - pointer to data block for large values

Plus: Most stack elements take exactly 16 bytes, only large values take more

Drawbacks
 - determining value size from implementation takes one pointer dereference (but we need dereference anyways to determine block/value)
 - more complex (value may be either a pointer or inline data)
 - still needs shifting, but shifting is relatively rare (only for value types > 8 bytes)

### More complex alternative

Datablock will not hold elements sequentially but holds a free list and allows malloc/free like access

Drawback: complex to implement for pure function calls

### Even more complex alternative

 - an array of types
 - an array of unions of (small value / pointer)

Layout
- For final value types, the union holds the value inline (up to 8 bytes)
- For final block types and objects, the union holds pointer to instance
- For large values it holds pointer to the value in datablock
- For non-final types it holds pointer to {Implementation, value} struct in datablock

This (almost) copies the native calling convention of az

Pluses
- block/value/object/interface/final are typecode flags, determined directly from typecode (no dereference)
- small primitives, final blocks and objects will take only 12 bytes

Drawbacks
- non-final block types will take 28 bytes
- complex to implement

### One more alternative

 - array of `const AZImplementation *`
 - array of `AZValue *`

But values use boxing - i.e. large values are allocated from heap (or from specialized pool)

Plus: easy to implement

Drawbacks
 - allocating boxed values from heap is not optimal in the middle of calculations
 - Still 24 bytes for stack entry

Or custom variant of this - do not use full AZValue but only 8-byte union. But then it needs custom BoxedValue type (the standard
one does not allow boxing anything <= 16 bytes)
Or relax the 16-byte boxing rule
Also for small boxes we can use pooled allocations
