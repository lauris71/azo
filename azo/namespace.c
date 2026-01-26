#define __AZO_NAMESPACE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2021
*/

#include <stdlib.h>

#include <arikkei/arikkei-utils.h>

#include <az/boxed-value.h>
#include <az/string.h>
#include <az/extend.h>

#include <azo/namespace.h>

static void namespace_class_init (AZONamespaceClass *klass);
static void namespace_init (AZONamespaceClass *klass, AZONamespace *nspace);

static unsigned int namespace_get_size (const AZCollectionImplementation *coll_impl, void *coll_inst);
static unsigned int namespace_contains (const AZCollectionImplementation *coll_impl, void *coll_inst, const AZImplementation *impl, const void *inst);
static const AZImplementation *namespace_val_get_element (const AZListImplementation *list_impl, void *list_inst, unsigned int idx, AZValue *val, unsigned int size);
static unsigned int namespace_keys_contains (const AZCollectionImplementation *coll_impl, void *coll_inst, const AZImplementation *impl, const void *inst);
static const AZImplementation *namespace_keys_get_element (const AZListImplementation *list_impl, void *list_inst, unsigned int idx, AZValue *val, unsigned int size);

const AZImplementation *namespace_lookup (const AZAttribDictImplementation *attrd_impl, void *attrd_inst, const AZString *key, AZValue *val, int size, unsigned int *flags);
unsigned int namespace_set (const AZAttribDictImplementation *attrd_impl, void *attrd_inst, AZString *key, const AZImplementation *impl, void *inst, unsigned int flags);

static unsigned int azo_namespace_type = 0;
static AZONamespaceClass *azo_namespace_class;

unsigned int
azo_namespace_get_type (void)
{
	if (!azo_namespace_type) {
		az_register_type (&azo_namespace_type, (const unsigned char *) "AZONamespace", AZ_TYPE_BLOCK, sizeof (AZONamespaceClass), sizeof (AZONamespace), AZ_FLAG_ZERO_MEMORY | AZ_FLAG_FINAL,
			(void (*) (AZClass *)) namespace_class_init,
			(void (*) (const AZImplementation *, void *)) namespace_init,
			NULL);
		azo_namespace_class = (AZONamespaceClass *) az_type_get_class (azo_namespace_type);
	}
	return azo_namespace_type;
}

static void
namespace_class_init (AZONamespaceClass *klass)
{
	az_class_set_num_interfaces ((AZClass *)klass, 1);
	az_class_declare_interface ((AZClass *) klass, 0, AZ_TYPE_ATTRIBUTE_DICT, ARIKKEI_OFFSET (AZONamespaceClass, attrd_impl), 0);
	klass->attrd_impl.map_impl.collection_impl.get_size = namespace_get_size;
	klass->attrd_impl.map_impl.collection_impl.contains = namespace_contains;
	klass->attrd_impl.val_list_impl.get_element = namespace_val_get_element;
	klass->attrd_impl.key_list_impl.collection_impl.contains = namespace_keys_contains;
	klass->attrd_impl.key_list_impl.get_element = namespace_keys_get_element;
	klass->attrd_impl.lookup = namespace_lookup;
	klass->attrd_impl.set = namespace_set;
}

static void
namespace_init (AZONamespaceClass *klass, AZONamespace *nspace)
{
	nspace->size = 32;
	nspace->entries = (AZONamespaceEntry *) malloc (nspace->size * sizeof (AZONamespaceEntry));
}

static unsigned int
namespace_get_size (const AZCollectionImplementation *coll_impl, void *coll_inst)
{
	AZONamespace *nspace = (AZONamespace *) coll_inst;
	return nspace->length;
}

static unsigned int
namespace_contains (const AZCollectionImplementation *coll_impl, void *coll_inst, const AZImplementation *impl, const void *inst)
{
	unsigned int i;
	AZONamespace *nspace = (AZONamespace *) coll_inst;
	for (i = 0; i < nspace->length; i++) {
		if (nspace->entries[i].val.impl != impl) continue;
		if (az_value_equals_instance (impl, &nspace->entries[i].val.v.value, inst)) return 1;
	}
	return 0;
}
static const AZImplementation *
namespace_val_get_element (const AZListImplementation *list_impl, void *list_inst, unsigned int idx, AZValue *val, unsigned int size)
{
	AZONamespace *nspace = (AZONamespace *) list_inst;
	return az_value_copy_autobox(nspace->entries[idx].val.impl, val, &nspace->entries[idx].val.v.value, size);
}

static unsigned int
namespace_keys_contains (const AZCollectionImplementation *coll_impl, void *coll_inst, const AZImplementation *impl, const void *inst)
{
	unsigned int i;
	arikkei_return_val_if_fail (AZ_IMPL_TYPE(impl) != AZ_TYPE_STRING, 0);
	AZONamespace *nspace = (AZONamespace *) coll_inst;
	for (i = 0; i < nspace->length; i++) {
		if (nspace->entries[i].key == inst) return 1;
	}
	return 0;
}

static const AZImplementation *
namespace_keys_get_element (const AZListImplementation *list_impl, void *list_inst, unsigned int idx, AZValue *val, unsigned int size)
{
	AZONamespace *nspace = (AZONamespace *) list_inst;
	az_value_set_reference(val, &nspace->entries[idx].key->reference);
	return (AZImplementation *) &AZStringKlass;
}

const AZImplementation *
namespace_lookup (const AZAttribDictImplementation *attrd_impl, void *attrd_inst, const AZString *key, AZValue *val, int size, unsigned int *flags)
{
	unsigned int i;
	AZONamespace *nspace = (AZONamespace *) attrd_inst;
	for (i = 0; i < nspace->length; i++) {
		if (nspace->entries[i].key == key) {
			*flags = nspace->entries[i].flags;
			return az_value_copy_autobox (nspace->entries[i].val.impl, val, &nspace->entries[i].val.v.value, size);
			return nspace->entries[i].val.impl;
		}
	}
	*flags = 0;
	return 0;
}

unsigned int
namespace_set (const AZAttribDictImplementation *attrd_impl, void *attrd_inst, AZString *key, const AZImplementation *impl, void *inst, unsigned int flags)
{
	unsigned int i;
	AZONamespace *nspace = (AZONamespace *) attrd_inst;
	for (i = 0; i < nspace->length; i++) {
		arikkei_return_val_if_fail (nspace->entries[i].key != key, 0);
	}
	if (nspace->length >= nspace->size) {
		nspace->size = nspace->size << 1;
		nspace->entries = (AZONamespaceEntry *) realloc (nspace->entries, nspace->size * sizeof (AZONamespaceEntry));
	}
	az_string_ref (key);
	nspace->entries[nspace->length].key = key;
	nspace->entries[nspace->length].flags = flags;
	nspace->entries[nspace->length].val.impl = impl;
	az_packed_value_set_from_impl_instance (&nspace->entries[nspace->length].val.packed_val, impl, inst);
	nspace->length += 1;
	return 1;
}

unsigned int
azo_namespace_define (AZONamespace *nspace, AZString *key, const AZImplementation *impl, void *inst)
{
	return az_attrib_dict_set (&azo_namespace_class->attrd_impl, nspace, key, impl, inst, AZ_ATTRIB_ARRAY_IS_FINAL);
}

unsigned int
azo_namespace_define_by_str (AZONamespace *nspace, const unsigned char *key, const AZImplementation *impl, void *inst)
{
	unsigned int result;
	AZString *str = az_string_new (key);
	result = azo_namespace_define (nspace, str, impl, inst);
	az_string_unref (str);
	return result;
}

unsigned int
azo_namespace_define_by_type (AZONamespace *nspace, AZString *key, unsigned int type, void *inst)
{
	return azo_namespace_define (nspace, key, (const AZImplementation *) az_type_get_class (type), inst);
}

unsigned int
azo_namespace_define_by_str_type (AZONamespace *nspace, const unsigned char *key, unsigned int type, void *inst)
{
	return azo_namespace_define_by_str (nspace, key, ( const AZImplementation *) az_type_get_class (type), inst);
}

unsigned int
azo_namespace_define_class (AZONamespace *nspace, AZString *key, unsigned int type)
{
	return azo_namespace_define_by_type (nspace, key, AZ_TYPE_CLASS, az_type_get_class (type));
}

unsigned int
azo_namespace_define_class_by_str (AZONamespace *nspace, const unsigned char *key, unsigned int type)
{
	return azo_namespace_define_by_str_type (nspace, key, AZ_TYPE_CLASS, az_type_get_class (type));
}
