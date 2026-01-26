#define __AZO_SOURCE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2019
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <az/extend.h>

#include "source.h"

static void source_finalize (AZOSourceClass *klass, AZOSource *source);

static unsigned int source_type = 0;

unsigned int
azo_source_get_type (void)
{
	if (!source_type) {
		az_register_type (&source_type, (const unsigned char *) "AZOSource", AZ_TYPE_REFERENCE, sizeof (AZOSourceClass), sizeof (AZOSource), AZ_FLAG_FINAL | AZ_FLAG_ZERO_MEMORY,
			NULL, NULL,
			(void (*) (const AZImplementation *, void *)) source_finalize);
	}
	return source_type;
}

static void
source_finalize (AZOSourceClass *klass, AZOSource *src)
{
	if (!src->is_static) free ((void *) src->cdata);
	if (src->lines) free(src->lines);
}

AZOSource *
azo_source_new_transfer (uint8_t *cdata, unsigned int csize)
{
	AZOSource *src = az_instance_new (AZO_TYPE_SOURCE);
	src->cdata = cdata;
	src->csize = csize;
	return src;
}

AZOSource *
azo_source_new_duplicate (const uint8_t *cdata, unsigned int csize)
{
	AZOSource *src = az_instance_new (AZO_TYPE_SOURCE);
	src->cdata = (const unsigned char *) malloc (csize);
	memcpy ((void *) src->cdata, cdata, csize);
	src->csize = csize;
	return src;
}

AZOSource *
azo_source_new_static (const uint8_t *cdata, unsigned int csize)
{
	AZOSource *src = az_instance_new (AZO_TYPE_SOURCE);
	src->cdata = cdata;
	src->csize = csize;
	src->is_static = 1;
	return src;
}

static void
azo_source_find_lines(AZOSource *src) {
	src->n_lines = 1;
	for (unsigned int p = 0; p < src->csize; p++) if (src->cdata[p] == '\n') src->n_lines += 1;
	src->lines = (unsigned int *) malloc(src->n_lines * sizeof(unsigned int));
	unsigned int l_idx = 0;
	src->lines[l_idx++] = 0;
	for (unsigned int p = 0; p < src->csize; p++) if (src->cdata[p] == '\n') src->lines[l_idx++] = p + 1;
}

void
azo_source_ensure_lines(AZOSource *src)
{
	if (!src->lines) azo_source_find_lines(src);
}

unsigned int
azo_source_find_line_range (AZOSource *src, unsigned int start, unsigned int end, unsigned int *first, unsigned int *last)
{
	unsigned int i, f, l;
	azo_source_ensure_lines(src);
	f = l = src->n_lines;
	for (i = 0; i < src->n_lines; i++) {
		unsigned int line_start = src->lines[i];
		unsigned int line_end = (i < (src->n_lines - 1)) ? src->lines[i + 1] : src->csize;
		if ((line_start <= start) && (line_end > start)) f = i;
		if ((line_start <= (end - 1)) && (line_end > (end - 1))) l = i;
	}
	if ((f < src->n_lines) && (l < src->n_lines)) {
		*first = f;
		*last = l;
		return 1;
	}
	return 0;
}

void
azo_source_print_lines (AZOSource *src, unsigned int start, unsigned int end)
{
	azo_source_ensure_lines(src);
	for (unsigned int l = start; l < end; l++) {
		unsigned int s = src->lines[l];
		unsigned int e = ((l + 1) < src->n_lines) ? src->lines[l + 1] : src->csize;
		for (unsigned int c = s; c < e; c++) {
			fprintf (stderr, "%c", src->cdata[c]);
		}
		if (l == (src->n_lines - 1)) fprintf (stderr, "\n");
	}
}
