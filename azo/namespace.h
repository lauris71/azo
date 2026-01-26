#ifndef __AZO_NAMESPACE_H__
#define __AZO_NAMESPACE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#define AZO_TYPE_NAMESPACE azo_namespace_get_type()

typedef struct _AZONamespace AZONamespace;
typedef struct _AZONamespaceClass AZONamespaceClass;
typedef struct _AZONamespaceEntry AZONamespaceEntry;

#include <az/classes/attrib-dict.h>
#include <az/packed-value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZONamespaceEntry {
	AZString *key;
	unsigned int flags;
	unsigned int filler;
	AZPackedValue64 val;
};

struct _AZONamespace {
	unsigned int length;
	unsigned int size;
	AZONamespaceEntry *entries;
};

struct _AZONamespaceClass {
	AZClass klass;
	AZAttribDictImplementation attrd_impl;
};

unsigned int azo_namespace_get_type (void);

unsigned int azo_namespace_define (AZONamespace *nspace, AZString *key, const AZImplementation *impl, void *inst);
unsigned int azo_namespace_define_by_str (AZONamespace *nspace, const unsigned char *key, const AZImplementation *impl, void *inst);
unsigned int azo_namespace_define_by_type (AZONamespace *nspace, AZString *key, unsigned int type, void *inst);
unsigned int azo_namespace_define_by_str_type (AZONamespace *nspace, const unsigned char *key, unsigned int type, void *inst);
unsigned int azo_namespace_define_class (AZONamespace *nspace, AZString *key, unsigned int type);
unsigned int azo_namespace_define_class_by_str (AZONamespace *nspace, const unsigned char *key, unsigned int type);

#ifdef __cplusplus
}
#endif

#endif

