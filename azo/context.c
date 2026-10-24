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
#include <az/collections/map.h>
#include <az/string.h>
#include <az/value.h>
#include <az/extend.h>

#include "context.h"
#include "namespace.h"
#include "interpreter.h"
#include <azo/defer.h>

struct _AZOContextPriv {
	ArikkeiDict definitions;
	unsigned int values_size;
	unsigned int nvalues;
	AZPackedValue *values;
	AZString **keys;
};

AZOContext *
azo_context_new (void)
{
	/* fixme: This is lazily initialized and eccessed through class */
	azo_defer_handle_get_type();

	AZOContext *ctx = (AZOContext *) calloc(1, sizeof(AZOContext));
	ctx->priv = (AZOContextPriv *) calloc(1, sizeof(AZOContextPriv));

	arikkei_dict_setup_string (&ctx->priv->definitions, 301);
	ctx->priv->values_size = 16;
	ctx->priv->values = (AZPackedValue *) malloc (ctx->priv->values_size * sizeof (AZPackedValue));
	memset (ctx->priv->values, 0, ctx->priv->values_size * sizeof (AZPackedValue));
	ctx->priv->keys = (AZString **) malloc (ctx->priv->values_size * sizeof (AZString *));

	ctx->intr = azo_interpreter_new (ctx);

	return ctx;
}

void
azo_context_delete (AZOContext *ctx)
{
	unsigned int i;
	for (i = 0; i < ctx->priv->nvalues; i++) {
		az_packed_value_clear (&ctx->priv->values[i]);
		az_string_unref (ctx->priv->keys[i]);
	}
	free (ctx->priv->values);
	free (ctx->priv->keys);
	arikkei_dict_release (&ctx->priv->definitions);
	free(ctx->priv);

	free(ctx);
}

static const char *excnames[] = {
	NULL,
	/* az */
	"System failure",
	"Out of memory",
	"Null dereference",
	"Invalid type",
	"Invalid value",
	"Out of bounds",
	/* azo */
	"Change of magnitude",
	"Change of precision",
	"Non-finite result",
	"Integer overflow",
	"Stack overflow",
	"Invalid conversion",
	"Invalid property",
	"Divide by zero",
	"Not writable"
};

void
azo_context_set_exception(AZOContext *ctx, unsigned int type, unsigned int mask, const uint8_t *ipc)
{
	az_exception_set(&ctx->az_ctx.exc, type, mask, (const uint8_t *) excnames[type]);
	ctx->az_ctx.exc.level = AZO_EXCEPTION_LEVEL;
	ctx->exc_ip = ipc;
}

void
azo_context_clear_exception(AZOContext *ctx)
{
	az_exception_clear(&ctx->az_ctx.exc);
	ctx->exc_ip = NULL;
}

unsigned int
azo_context_define(AZOContext *ctx, AZString *key, const AZImplementation *impl, void *inst)
{
	AZOContextFull *fctx = (AZOContextFull *) ctx;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (impl != NULL, 0);
	arikkei_return_val_if_fail (inst != NULL, 0);
	if (arikkei_dict_exists_pval (&ctx->priv->definitions, key->str)) return 0;
	if (ctx->priv->nvalues >= ctx->priv->values_size) {
		unsigned int newsize = ctx->priv->values_size << 1;
		ctx->priv->values = (AZPackedValue *) realloc (ctx->priv->values, newsize * sizeof (AZPackedValue));
		memset (&ctx->priv->values[ctx->priv->values_size], 0, (newsize - ctx->priv->values_size) * sizeof (AZPackedValue));
		ctx->priv->keys = (AZString **) realloc (ctx->priv->keys, newsize * sizeof (AZString *));
		ctx->priv->values_size = newsize;
	}
	void *valptr = ARIKKEI_INT_TO_POINTER(ctx->priv->nvalues);
	arikkei_dict_insert_pval (&ctx->priv->definitions, key->str, valptr);
	az_packed_value_set_autobox(&ctx->priv->values[ctx->priv->nvalues], impl, inst);
	az_string_ref (key);
	ctx->priv->keys[ctx->priv->nvalues++] = key;
	return 1;
}

unsigned int
azo_context_define_by_str (AZOContext *ctx, const unsigned char *key, const AZImplementation *impl, void *inst)
{
	AZString *str;
	unsigned int result;
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (impl != NULL, 0);
	arikkei_return_val_if_fail (inst != NULL, 0);
	str = az_string_new (key);
	result = azo_context_define (ctx, str, impl, inst);
	az_string_unref (str);
	return result;
}

const AZImplementation *
azo_context_lookup (AZOContext *ctx, AZString *key, AZValue *val, unsigned int size)
{
	arikkei_return_val_if_fail (ctx != NULL, 0);
	arikkei_return_val_if_fail (key != NULL, 0);
	arikkei_return_val_if_fail (val != NULL, 0);
	return azo_context_lookup_by_str (ctx, key->str, val, size);
}

const AZImplementation *
azo_context_lookup_by_str (AZOContext *ctx, const uint8_t *key, AZValue *val, unsigned int size)
{
	AZOContextFull *fctx = (AZOContextFull *) ctx;
	arikkei_return_val_if_fail (ctx != NULL, NULL);
	arikkei_return_val_if_fail (key != NULL, NULL);
	arikkei_return_val_if_fail (val != NULL, NULL);
	void **ptr = (void **) arikkei_dict_lookup (&ctx->priv->definitions, &key);
	if (!ptr) return NULL;
	unsigned int idx = ARIKKEI_POINTER_TO_INT(*ptr);
	return az_value_copy_autobox(ctx->priv->values[idx].impl, val, &ctx->priv->values[idx].v, size);
}

void
azo_context_define_basic_types (AZOContext *ctx)
{
	AZPackedValue val = { 0 };
	az_init ();
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
	azo_context_define_class_by_str (ctx, (const unsigned char *) "function", AZ_TYPE_FUNCTION);

	azo_context_define_class_by_str (ctx, (const unsigned char *) "array", AZ_TYPE_LIST);
	/* Namespaced members */
	AZONamespace *ns = azo_namespace_new ();
	azo_context_define_by_str(ctx, (const unsigned char *) "az", AZ_IMPL_FROM_TYPE(AZO_TYPE_NAMESPACE), ns);
	/* AZ subtypes */
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "object", AZ_TYPE_OBJECT);
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "collection", AZ_TYPE_COLLECTION);
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "array", AZ_TYPE_LIST);
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "map", AZ_TYPE_MAP);
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "ActiveObject", AZ_TYPE_ACTIVE_OBJECT);
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
	result = azo_context_define_by_str (ctx, key, AZ_IMPL_FROM_TYPE(AZ_TYPE_CLASS), klass);
	az_packed_value_clear (&val);
	return result;
}
