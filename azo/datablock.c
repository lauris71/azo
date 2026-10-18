#define __AZO_DATABLOCK_C__

#include <azo/datablock.h>

void
azo_datablock_init(AZODataBlock *block, unsigned int size_total)
{
    block->size_total = size_total;
    if (size_total) {
        block->entries = (AZODataBlockEntry *) calloc(size_total, sizeof(AZODataBlockEntry));
    } else {
        block->entries = NULL;
    }
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
    if (!block->size_total) return;
    for (unsigned int i = 0; i < block->size_total; i++) {
        AZODataBlockEntry *entry = &block->entries[i];
        azo_datablock_clear_entry(entry);
    }
    free(block->entries);
}

void
azo_datablock_clear(AZODataBlock *block)
{
    if (!block->size_total) return;
    for (unsigned int i = 0; i < block->size_total; i++) {
        AZODataBlockEntry *entry = &block->entries[i];
        azo_datablock_clear_entry(entry);
    }
    free(block->entries);
    block->entries = NULL;
    block->size_total = 0;
}

int
azo_datablock_set(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, void *inst, unsigned int flags)
{
    arikkei_return_val_if_fail(idx < block->size_total, AZ_INTERNAL_INCONSISTENCY);
    AZODataBlockEntry *entry = &block->entries[idx];
    if (entry->flags & AZO_DATABLOCK_FLAG_FINAL) {
        if (entry->flags & AZO_DATABLOCK_FLAG_SET) return AZ_VALUE_IS_FINAL;
    } else {
        azo_datablock_clear_entry(entry);
    }
    entry->impl = impl;
    if (impl) {
        if (AZ_IMPL_IS_REFERENCE(impl) && inst) {
            if (flags & AZO_DATABLOCK_FLAG_WEAK) {
                arikkei_return_val_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT), AZ_INVALID_TYPE);
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
    entry->flags |= AZO_DATABLOCK_FLAG_SET | (flags & AZO_DATABLOCK_FLAG_FINAL);
    return AZ_OK;
}

int
azo_datablock_set_from_val(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, const AZValue *val, unsigned int flags)
{
    arikkei_return_val_if_fail(idx < block->size_total, AZ_INTERNAL_INCONSISTENCY);
    AZODataBlockEntry *entry = &block->entries[idx];
    if (entry->flags & AZO_DATABLOCK_FLAG_FINAL) {
        if (entry->flags & AZO_DATABLOCK_FLAG_SET) return AZ_VALUE_IS_FINAL;
    } else {
        azo_datablock_clear_entry(entry);
    }
    entry->impl = impl;
    if (impl) {
        if (AZ_IMPL_IS_REFERENCE(impl) && (val->reference)) {
            if (flags & AZO_DATABLOCK_FLAG_WEAK) {
                arikkei_return_val_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT), AZ_INVALID_TYPE);
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
    entry->flags |= AZO_DATABLOCK_FLAG_SET | (flags & AZO_DATABLOCK_FLAG_FINAL);
    return AZ_OK;
}

int
azo_datablock_transfer_val(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, AZValue *val, unsigned int flags)
{
    int result = azo_datablock_set_from_val(block, idx, impl, val, flags);
    az_value_clear(impl, val);
    return result;
}

int
azo_datablock_set_weak(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, AZActiveObject *object)
{
    arikkei_return_val_if_fail(az_type_is_a(AZ_IMPL_TYPE(impl), AZ_TYPE_ACTIVE_OBJECT), AZ_INVALID_TYPE);
    return azo_datablock_set_from_val(block, idx, impl, (AZValue *) &object, AZO_DATABLOCK_FLAG_WEAK);
}

void
azo_datablock_print(AZODataBlock *block, FILE *ofs)
{
    for (unsigned int i = 0; i < block->size_total; i++) {
        AZODataBlockEntry *entry = &block->entries[i];
        fprintf(ofs, "Entry %u (%s %u): ", i, (entry->flags & AZO_DATABLOCK_FLAG_FINAL) ? "final" : "mutable", entry->flags);
        if (entry->impl) {
            uint8_t c[256];
            az_instance_to_string(entry->impl, az_value_get_inst(entry->impl, &entry->val), c, sizeof(c));
            fprintf(ofs, "%s\n", c);
        } else {
            fprintf(ofs, "null\n");
        }
    }
}
