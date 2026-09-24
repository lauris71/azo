#ifndef __AZO_DATABLOCK_H__
#define __AZO_DATABLOCK_H__
/*
 * A languge implementation based on AZ
 *
 * Copyright (C) Lauris Kaplinski 2026
 */

typedef struct _AZODataBlock AZODataBlock;
typedef struct _AZODataBlockEntry AZODataBlockEntry;

#define AZO_DATABLOCK_FLAG_SET 1
#define AZO_DATABLOCK_FLAG_WEAK 2

#include <stdint.h>

#include <az/value.h>
#include <az/weak-reference.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZODataBlockEntry {
    uint32_t flags;
    uint32_t filler;
    const AZImplementation *impl;
    union {
        AZValue val;
        AZWeakReference weak_ref;
    };
};

struct _AZODataBlock {
    unsigned int size_const;
    unsigned int size_total;
    AZODataBlockEntry *entries;
};

void azo_datablock_init(AZODataBlock *block, unsigned int size_const, unsigned int size_total);
void azo_datablock_finalize(AZODataBlock *block);

void azo_datablock_clear(AZODataBlock *block);

void azo_datablock_set(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, void *inst, unsigned int weak);
void azo_datablock_set_from_val(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, const AZValue *val, unsigned int weak);
void azo_datablock_transfer_val(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, AZValue *val, unsigned int weak);
void azo_datablock_set_weak(AZODataBlock *block, unsigned int idx, const AZImplementation *impl, AZActiveObject *object);

#ifdef __cplusplus
}
#endif

#endif
