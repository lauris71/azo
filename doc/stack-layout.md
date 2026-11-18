# Stack layout

## Prerequisites

For each type, the stack has to store both implementation and instance. The implementation provides runtime type information
for the cases where the type is not known at compile time.
The stack representation should be the same as the argument representation for the AZFunction `invoke` method to avoid copying arguments around.

Most stack elements are small primitives and block pointers. Complex double is the only primitive that does not fit
into 8 bytes (and is not commonly used). Other large value types are 3 and 4-component float vectors (12-16 bytes) and matrices (36, 48 or 64 bytes).

## The implemented design (az value store)

The stack is an array of `AZStoreElement` plus a data buffer of 16-byte granularity (az/value-store.h):

```c
struct _AZOStack {
	unsigned int size;         /* element capacity */
	unsigned int length;       /* element count */
	AZStoreElement *elements;
	unsigned int data_size;    /* data buffer capacity in 16-byte units */
	unsigned int data_pos;     /* data high-water mark in 16-byte units */
	AZValue *data;             /* 16-byte aligned data buffer */
};
```

Each `AZStoreElement` is 16 bytes: `{ const AZImplementation *impl; union { uint8_t val[8]; uint64_t val_idx; ... } }`

- `impl == NULL` - empty element (reserved local slot, untyped null)
- block classes - `val` holds the instance pointer directly (references are ref'd)
- values up to 8 bytes - inline in `val`
- larger values - `val_idx` indexes the data buffer in 16-byte units (`ceil(size / 16)` slots)

**The stack never holds boxed values**: `push_value`/`push_instance`/`push_value_transfer`/`store_transfer`
transparently unbox boxed values (the content is always a plain value and is stored inline or in the data
buffer; the box is released on transfer). Boxed *interfaces* are lifecycle guards and are kept as-is.

### Datablock management (lazy storage)

- **Append-only**: pushing a large value appends at `data_pos`. Mid-buffer holes are never scanned or reused.
- **Pop contraction**: popping frees the element's data slots; the high-water mark is lowered only across freed slots that touch it (`val_idx + blocks == data_pos`). Push/pop being LIFO, this reclaims the common case exactly.
- **Element operations never move data**: exchange is a plain 16-byte element swap, remove/roll are element-array memmoves. Transient holes below the high-water mark are reclaimed at frame rewind.
- **Slot store reuses storage**: overwriting a slot with a same-size-or-smaller large value reuses its `val_idx` (the common case for reassignment); otherwise a fresh slot is appended.
- **Frame rewind**: interpreter frames record `{elem_base, data_base}`; popping a frame hard-resets both, reclaiming all temporaries and transient holes of the frame in O(1).
- **Growth**: elements and data are reallocated with doubling. Because data slots are referenced by index, no pointer fixup is needed on realloc (this is the point of `val_idx` being an index).

### Conversion

Conversion happens into a 16-byte scratch and the result is stored back into the same element. The only storage-allocating
result is complex double (everything else fits inline or is a block pointer; there is no automatic conversion of custom structs),
so slot storage is never grown in place and no mid-buffer shifting ever occurs.

### Invoke ABI

Arguments are passed to `AZFunctionImplementation.invoke` as a pointer into the stack's element array plus the data buffer
(zero-copy); callees receive fully materialized argument slots. The old `invoke_vals` (impls/vals arrays) front-end remains for
transitional callers and translates via pointer emulation (`val_idx = ptr >> 4`, `data = NULL`).

## Properties vs the alternatives

- 16 bytes per stack element (vs 32+ in the old layout); one dereference less for block instances
- no intra-stack data shifting at all (exchange/roll/remove are O(elements))
- value/block distinction still needs one impl dereference (inherent to the store format)

## Considered alternatives (not implemented)

### The previous layout

 - array of `const AZImplementation *`
 - array of `AZValue *`
 - data block

Each stack entry took its `value_size` bytes (rounded up to 16) from the datablock, the value array pointed into it.
32+ bytes per element and intra-stack operations had to shift the data tail when value sizes differed.

### Union of small value / pointer per element, typecodes only

Like the store format, but non-final types hold a pointer to `{Implementation, value}` in the datablock and block/value/final
is read from typecode flags (saves the impl dereference). More complex; can be revisited if the impl dereference shows up in profiles.

### Free-list datablock

Malloc/free style datablock instead of high-water allocation. Much more complex for no measured need (frame rewind already
reclaims everything transient in O(1)).

### Boxed values for everything

All large values allocated from heap (or a pool). Simple, but per-value heap allocation in the middle of calculations is not acceptable.
