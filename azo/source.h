#ifndef __AZO_SOURCE_H__
#define __AZO_SOURCE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2019
*/

typedef struct _AZOSource AZOSource;
typedef struct _AZOSourceClass AZOSourceClass;

#define AZO_TYPE_SOURCE azo_source_get_type ()

#include <az/reference.h>
#include <az/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOSource {
	AZReference ref;
	const uint8_t *cdata;
	unsigned int csize;
	unsigned int is_static : 1;
	/* Line break-down */
	unsigned int n_lines;
	unsigned int *lines;
};

struct _AZOSourceClass {
	AZReferenceClass ref_klass;
};

unsigned int azo_source_get_type (void);

AZOSource *azo_source_new_transfer (uint8_t *cdata, unsigned int csize);
AZOSource *azo_source_new_duplicate (const uint8_t *cdata, unsigned int csize);
AZOSource *azo_source_new_static (const uint8_t *cdata, unsigned int csize);

static inline void
azo_source_ref(AZOSource *src)
{
	az_reference_ref((AZReference *) src);
}

static inline void
azo_source_unref(AZOSource *src)
{
	az_reference_unref((AZReferenceClass *) AZ_CLASS_FROM_TYPE(AZO_TYPE_SOURCE), (AZReference *) src);
}

void azo_source_ensure_lines(AZOSource *src);

unsigned int azo_source_find_line_range (AZOSource *src, unsigned int start, unsigned int end, unsigned int *first, unsigned int *last);

void azo_source_print_lines (AZOSource *src, unsigned int start, unsigned int end);

#ifdef __cplusplus
}
#endif

#endif
