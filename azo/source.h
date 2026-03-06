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

#include <az/object.h>
#include <az/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOSource {
	AZObject object;
	AZString *name;
	const uint8_t *cdata;
	unsigned int csize;
	unsigned int is_static : 1;
	/* Line break-down */
	unsigned int n_lines;
	unsigned int *lines;
};

struct _AZOSourceClass {
	AZObjectClass ref_klass;
};

unsigned int azo_source_get_type (void);

AZOSource *azo_source_new_transfer (const uint8_t *name, uint8_t *cdata, unsigned int csize);
AZOSource *azo_source_new_duplicate (const uint8_t *name, const uint8_t *cdata, unsigned int csize);
AZOSource *azo_source_new_static (const uint8_t *name, const uint8_t *cdata, unsigned int csize);

static inline void
azo_source_ref(AZOSource *src)
{
	az_object_ref((AZObject *) src);
}

static inline void
azo_source_unref(AZOSource *src)
{
	az_object_unref((AZObject *) src);
}

void azo_source_ensure_lines(AZOSource *src);

unsigned int azo_source_get_line_len(AZOSource *src, unsigned int line);
unsigned int azo_source_find_line_range (AZOSource *src, unsigned int start, unsigned int end, unsigned int *first, unsigned int *last);

void azo_source_print_lines (AZOSource *src, unsigned int start, unsigned int end);

#ifdef __cplusplus
}
#endif

#endif
