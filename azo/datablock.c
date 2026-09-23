#define __AZO_DATABLOCK_C__

#include <azo/datablock.h>

void
azo_datablock_init(AZODataBlock *block, unsigned int size_const, unsigned int size_total)
{
    block->size_const = size_const;
    block->size_total = size_total;
    block->entries = (AZODataBlockEntry *) calloc(size_total, sizeof(AZODataBlockEntry));
}

static void
azo_datablock_clear_entry(AZODataBlockEntry *entry)
{
    if (entry->impl) {
        if (AZ_IMPL_IS_REFERENCE(entry->impl) && (entry->val.reference)) {
            if (entry->flags & AZO_DATABLOCK_FLAG_WEAK) {       
                az_weak_reference_clear(&entry->weak_ref);
            } else {
                az_reference_unref((AZReferenceClass *) entry->impl, entry->val.reference);
            }
        }
        entry->impl = NULL;
    }
    entry->flags = 0;
}

void
azo_datablock_finalize(AZODataBlock *block)
{
    for (unsigned int i = 0; i < block->size_total; i++) {
        AZODataBlockEntry *entry = &block->entries[i];
        azo_datablock_clear_entry(entry);
    }
    free(block->entries);
}

void
azo_datablock_set(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, void *inst, unsigned int weak)
{
    arikkei_return_if_fail(idx < block->size_total);
    AZODataBlockEntry *entry = &block->entries[idx];
    if (idx < block->size_const) {
        arikkei_return_if_fail(!(entry->flags & AZO_DATABLOCK_FLAG_SET));
    } else {
        azo_datablock_clear_entry(entry);
    }
    entry->impl = impl;
    if (impl) {
        if (AZ_IMPL_IS_REFERENCE(impl) && inst) {
            if (weak) {
                arikkei_return_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT));
                az_weak_reference_set(&entry->weak_ref, (AZActiveObject *) inst);
                entry->flags = AZO_DATABLOCK_FLAG_WEAK;
            } else {
                entry->val.reference = (AZReference *) inst;
                az_reference_ref(entry->val.reference);
            }
        } else {
            az_value_set_from_inst_autobox(impl, &entry->val, AZ_VALUE_MAX_SIZE, inst);
        }
    }
    entry->flags |= AZO_DATABLOCK_FLAG_SET;
}

void
azo_datablock_set_from_val(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, const AZValue *val, unsigned int weak)
{
    arikkei_return_if_fail(idx < block->size_total);
    AZODataBlockEntry *entry = &block->entries[idx];
    if (idx < block->size_const) {
        arikkei_return_if_fail(!(entry->flags & AZO_DATABLOCK_FLAG_SET));
    } else {
        azo_datablock_clear_entry(entry);
    }
    entry->impl = impl;
    if (impl) {
        if (AZ_IMPL_IS_REFERENCE(impl) && (val->reference)) {
            if (weak) {
                arikkei_return_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT));
                az_weak_reference_set(&entry->weak_ref, (AZActiveObject *) val->reference);
                entry->flags = AZO_DATABLOCK_FLAG_WEAK;
            } else {
                entry->val.reference = val->reference;
                az_reference_ref(entry->val.reference);
            }
        } else {
            entry->val = *val;
        }
    }
    entry->flags |= AZO_DATABLOCK_FLAG_SET;
}

void
azo_datablock_set_weak(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, AZActiveObject *object)
{
    arikkei_return_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT));
    azo_datablock_set_from_val(block, idx, impl, (AZValue *) &object, 1);
}
