#define __AZO_EXECUTION_CONTEXT_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOContextFull AZOContextFull;

#include <stdlib.h>
#include <string.h>

#include <arikkei/arikkei-dict.h>
#include <arikkei/arikkei-utils.h>

#include <az/classes/active-object.h>
#include <az/collections/list.h>
#include <az/classes/map.h>
#include <az/string.h>
#include <az/value.h>
#include <az/extend.h>

#include "context.h"
#include "interpreter.h"

struct _AZOContextFull {
	AZOContext azo_ctx;
	ArikkeiDict definitions;
	unsigned int values_size;
	unsigned int nvalues;
	AZPackedValue *values;
	AZString **keys;
};

struct _AZOContextClass {
	AZClass block_class;
};

static void context_class_init (AZOContextClass *klass);
static void context_init (AZOContextClass *klass, AZOContextFull *fctx);
static void context_finalize (AZOContextClass *klass, AZOContextFull *fctx);

/* Method implementations */
static unsigned int context_call_define (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx);

enum {
	/* Functions */
	FUNC_DEFINE,
	NUM_FUNCTIONS,
	/* Values */
	NUM_PROPERTIES = NUM_FUNCTIONS
};

unsigned int
azo_context_get_type (void)
{
	static unsigned int type = 0;
	if (!type) {
		az_register_type (&type, (const unsigned char *) "AZOContext", AZ_TYPE_BLOCK, sizeof (AZOContextClass), sizeof (AZOContextFull), AZ_FLAG_FINAL | AZ_FLAG_ZERO_MEMORY,
			(void (*) (AZClass *)) context_class_init,
			(void (*) (const AZImplementation *, void *)) context_init,
			(void (*) (const AZImplementation *, void *)) context_finalize);
	}
	return type;
}

static void
context_class_init (AZOContextClass *klass)
{
	az_class_set_num_properties ((AZClass *) klass, NUM_PROPERTIES);
	/* Functions */
	az_class_define_method_va ((AZClass *) klass, FUNC_DEFINE, (const unsigned char *) "define", context_call_define, AZ_TYPE_NONE, 0);
}

static void
context_init (AZOContextClass *klass, AZOContextFull *fctx)
{
	arikkei_dict_setup_string (&fctx->definitions, 301);
	fctx->values_size = 16;
	fctx->values = (AZPackedValue *) malloc (fctx->values_size * sizeof (AZPackedValue));
	memset (fctx->values, 0, fctx->values_size * sizeof (AZPackedValue));
	fctx->keys = (AZString **) malloc (fctx->values_size * sizeof (AZString *));
	fctx->azo_ctx.intr = azo_interpreter_new (&fctx->azo_ctx);
}

static void
context_finalize (AZOContextClass *klass, AZOContextFull *fctx)
{
	unsigned int i;
	for (i = 0; i < fctx->nvalues; i++) {
		az_packed_value_clear (&fctx->values[i]);
		az_string_unref (fctx->keys[i]);
	}
	free (fctx->values);
	free (fctx->keys);
	arikkei_dict_release (&fctx->definitions);
}

static unsigned int
context_call_define (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx)
{
	return 0;
}

AZOContext *
azo_context_new (void)
{
	AZOContext *ctx = (AZOContext *) az_instance_new (AZO_TYPE_CONTEXT);
	return ctx;
}

void
azo_context_delete (AZOContext *ctx)
{
	az_instance_delete (AZO_TYPE_CONTEXT, ctx);
}

unsigned int
azo_context_define (AZOContext *ctx, AZString *key, const AZPackedValue *value)
{
	AZOContextFull *fctx = (AZOContextFull *) ctx;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (value != NULL, 0);
	if (arikkei_dict_exists (&fctx->definitions, key->str)) return 0;
	if (fctx->nvalues >= fctx->values_size) {
		unsigned int newsize = fctx->values_size << 1;
		fctx->values = (AZPackedValue *) realloc (fctx->values, newsize * sizeof (AZPackedValue));
		memset (&fctx->values[fctx->values_size], 0, (newsize - fctx->values_size) * sizeof (AZPackedValue));
		fctx->keys = (AZString **) realloc (fctx->keys, newsize * sizeof (AZString *));
		fctx->values_size = newsize;
	}
	arikkei_dict_insert (&fctx->definitions, key->str, ARIKKEI_INT_TO_POINTER (fctx->nvalues));
	az_packed_value_copy (&fctx->values[fctx->nvalues], value);
	az_string_ref (key);
	fctx->keys[fctx->nvalues++] = key;
	return 1;
}

unsigned int
azo_context_define_by_str (AZOContext *ctx, const unsigned char *key, const AZPackedValue *value)
{
	AZString *str;
	unsigned int result;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (value != NULL, 0);
	str = az_string_new (key);
	result = azo_context_define (ctx, str, value);
	az_string_unref (str);
	return result;
}

unsigned int
azo_context_lookup (AZOContext *ctx, AZString *key, AZPackedValue *value)
{
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (value != NULL, 0);
	return azo_context_lookup_by_str (ctx, key->str, value);
}

unsigned int
azo_context_lookup_by_str (AZOContext *ctx, const unsigned char *key, AZPackedValue *value)
{
	AZOContextFull *fctx = (AZOContextFull *) ctx;
	unsigned int idx;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (value != NULL, 0);
	if (!arikkei_dict_exists (&fctx->definitions, key)) return 0;
	idx = ARIKKEI_POINTER_TO_INT (arikkei_dict_lookup (&fctx->definitions, key));
	az_packed_value_copy (value, &fctx->values[idx]);
	return 1;
}

void
azo_context_define_basic_types (AZOContext *ctx)
{
	AZPackedValue val = { 0 };
	az_init ();
	/* None */
	azo_context_define_by_str (ctx, (const unsigned char *) "none", &val);
	az_packed_value_clear (&val);
	/* Basic types */
	azo_context_define_class_by_str (ctx, (const unsigned char *) "any", AZ_TYPE_ANY);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "boolean", AZ_TYPE_BOOLEAN);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "int8", AZ_TYPE_INT8);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "uint8", AZ_TYPE_UINT8);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "int16", AZ_TYPE_INT16);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "uint16", AZ_TYPE_UINT16);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "int32", AZ_TYPE_INT32);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "uint32", AZ_TYPE_UINT32);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "int64", AZ_TYPE_INT64);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "uint64", AZ_TYPE_UINT64);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "float", AZ_TYPE_FLOAT);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "double", AZ_TYPE_DOUBLE);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "pointer", AZ_TYPE_POINTER);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "struct", AZ_TYPE_STRUCT);
	/* Special */
	azo_context_define_class_by_str (ctx, (const unsigned char *) "class", AZ_TYPE_CLASS);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "interface", AZ_TYPE_INTERFACE);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "reference", AZ_TYPE_REFERENCE);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "string", AZ_TYPE_STRING);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "object", AZ_TYPE_OBJECT);
	/* AZ subtypes */
	azo_context_define_class_by_str (ctx, (const unsigned char *) "function", AZ_TYPE_FUNCTION);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "collection", AZ_TYPE_COLLECTION);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "array", AZ_TYPE_LIST);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "map", AZ_TYPE_MAP);
	azo_context_define_class_by_str (ctx, (const unsigned char *) "ActiveObject", AZ_TYPE_ACTIVE_OBJECT);
}

unsigned int
azo_context_define_class_by_str (AZOContext *ctx, const unsigned char *key, unsigned int type)
{
	AZClass *klass;
	AZPackedValue val = { 0 };
	unsigned int result;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (az_type_is_a (type, AZ_TYPE_ANY), 0);
	klass = az_type_get_class (type);
	az_packed_value_set_class (&val, klass);
	result = azo_context_define_by_str (ctx, key, &val);
	az_packed_value_clear (&val);
	return result;
}
