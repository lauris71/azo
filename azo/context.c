#define __AZO_EXECUTION_CONTEXT_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

typedef struct _AZOContextFull AZOContextFull;

#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

typedef struct _AZOCtxDef AZOCtxDef;
typedef struct _AZOCtxExcEntry AZOCtxExcEntry;

/*
 * Type dictionary entry (may contain types/namespaces)
 *
 * NB! Strings are hashed/compared by pointer, thus the queries must used &key
 */
struct _AZOCtxDef {
	ArikkeiDictEntry entry;
	uint32_t filler;
	AZString *name;
	AZPackedValue value;
};

static void
def_copy_key (void *dst, const void *src)
{
	AZString **dst_str = (AZString **) dst;
	AZString **src_str = (AZString **) src;
	*dst_str = *src_str;
	az_string_ref(*src_str);
}

static void
def_free_key (void *key)
{
	AZString **key_str = (AZString **) key;
	az_string_unref(*key_str);
}

void
def_copy_value (void *dst, const void *src)
{
	AZPackedValue *dst_val = (AZPackedValue *) dst;
	AZPackedValue *src_val = (AZPackedValue *) src;
	/* Dict reallocation initializes to zero so we can safely copy */
	az_packed_value_copy(dst_val, src_val);
}

static void
def_free_value (void *value)
{
	AZPackedValue *val = (AZPackedValue *) value;
	az_packed_value_clear(val);
}

/*
 * Exception dictionary entry
 *
 * NB! Strings are hashed/compared by pointer, thus the queries must used &key
 */

struct _AZOCtxExcEntry {
	ArikkeiDictEntry entry;
	uint32_t filler;
	AZString *name;
	AZOExceptionDef def;
};

void
exc_entry_copy_value (void *dst, const void *src)
{
	memcpy(dst, src, sizeof(AZOExceptionDef));
}

struct _AZOContextPriv {
	ArikkeiDict definitions;
	ArikkeiDict exc_defs;
	unsigned int next_exc_type;
	unsigned int next_category_bit;
};

AZOContext *
azo_context_new (void)
{
	/* fixme: This is lazily initialized and accessed through class pointer */
	azo_defer_handle_get_type();

	AZOContext *ctx = (AZOContext *) calloc(1, sizeof(AZOContext));
	ctx->priv = (AZOContextPriv *) calloc(1, sizeof(AZOContextPriv));
	ctx->priv->next_exc_type = 1;

	ctx->priv->definitions.key_offset = 8;
	ctx->priv->definitions.key_size = 8;
	ctx->priv->definitions.val_offset = 16;
	ctx->priv->definitions.val_size = 32;
	ctx->priv->definitions.hash = arikkei_pointer_hash;
	ctx->priv->definitions.equal = arikkei_pointer_equal;
	ctx->priv->definitions.copy_key = def_copy_key;
	ctx->priv->definitions.free_key = def_free_key;
	ctx->priv->definitions.copy_value = def_copy_value;
	ctx->priv->definitions.free_value = def_free_value;
	arikkei_dict_setup(&ctx->priv->definitions, 301);

	ctx->priv->exc_defs.key_offset = 8;
	ctx->priv->exc_defs.key_size = 8;
	ctx->priv->exc_defs.val_offset = 16;
	ctx->priv->exc_defs.val_size = 16;
	ctx->priv->exc_defs.hash = arikkei_pointer_hash;
	ctx->priv->exc_defs.equal = arikkei_pointer_equal;
	ctx->priv->exc_defs.copy_key = def_copy_key;
	ctx->priv->exc_defs.free_key = def_free_key;
	ctx->priv->exc_defs.copy_value = exc_entry_copy_value;
	ctx->priv->exc_defs.free_value = NULL;
	arikkei_dict_setup(&ctx->priv->exc_defs, 31);

	ctx->intr = azo_interpreter_new (ctx);

	return ctx;
}

void
azo_context_delete (AZOContext *ctx)
{
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
	if (arikkei_dict_exists(&ctx->priv->definitions, &key)) return 0;
	AZPackedValue val = {0};
	az_packed_value_set_autobox(&val, impl, inst);
	arikkei_dict_insert(&ctx->priv->definitions, &key, &val);
	az_packed_value_clear(&val);
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
	const AZPackedValue *pval = (const AZPackedValue *) arikkei_dict_lookup(&ctx->priv->definitions, &key);
	if (!pval) return NULL;
	return az_value_copy_autobox(pval->impl, val, &pval->v, size);
}

const AZImplementation *
azo_context_lookup_by_str (AZOContext *ctx, const uint8_t *key, AZValue *val, unsigned int size)
{
	AZOContextFull *fctx = (AZOContextFull *) ctx;
	arikkei_return_val_if_fail (ctx != NULL, NULL);
	arikkei_return_val_if_fail (key != NULL, NULL);
	arikkei_return_val_if_fail (val != NULL, NULL);
	AZString *str = az_string_new (key);
	const AZImplementation *impl = azo_context_lookup(ctx, str, val, size);
	az_string_unref(str);
	return impl;
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

unsigned int
azo_context_register_exception(AZOContext *ctx, AZString *name, uint64_t category)
{
	AZOExceptionDef *def = (AZOExceptionDef *) arikkei_dict_lookup(&ctx->priv->exc_defs, &name);
	if (def) {
		if (def->category != category) {
			fprintf(stderr, "Exception category does not match previus definiton: %s %llu != %llu\n", name->str, category, def->category);
			return AZ_EXCEPTION_NONE;
		}
		return def->type;
	}
	AZOExceptionDef new_def = {0};
	new_def.type = ctx->priv->next_exc_type++;
	new_def.category = category;
	arikkei_dict_insert(&ctx->priv->exc_defs, &name, &new_def);
	return new_def.type;
}

uint64_t
azo_context_register_exception_category(AZOContext *ctx, AZString *name)
{
	AZOExceptionDef *def = (AZOExceptionDef *) arikkei_dict_lookup(&ctx->priv->exc_defs, &name);
	if (def) {
		if (def->type != AZ_EXCEPTION_NONE) {
			fprintf(stderr, "Invalid category name: %s is exception %u\n", name->str, def->type);
			return 0;
		}
		return def->category;
	}
	AZOExceptionDef new_def = {0};
	new_def.type = AZ_EXCEPTION_NONE;
	new_def.category = 1ULL << ctx->priv->next_category_bit++;
	arikkei_dict_insert(&ctx->priv->exc_defs, &name, &new_def);
	return new_def.category;
}

static unsigned int
azo_context_register_exception_by_str(AZOContext *ctx, const uint8_t *name, uint64_t category)
{
	AZString *str = az_string_new (name);
	unsigned int result = azo_context_register_exception (ctx, str, category);
	az_string_unref (str);
	return result;
}

static uint64_t
azo_context_register_exception_category_by_str(AZOContext *ctx, const uint8_t *name)
{
	AZString *str = az_string_new (name);
	uint64_t result = azo_context_register_exception_category (ctx, str);
	az_string_unref (str);
	return result;
}

const AZOExceptionDef *
azo_context_lookup_exception(AZOContext *ctx, AZString *name)
{
	return (const AZOExceptionDef *) arikkei_dict_lookup(&ctx->priv->exc_defs, &name);
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

	/* Exceptions */
	/* Categories */
	uint64_t cat_system = azo_context_register_exception_category_by_str(ctx, (const uint8_t *) "SystemException");
	uint64_t cat_runtime = azo_context_register_exception_category_by_str(ctx, (const uint8_t *) "RuntimeException");
	uint64_t cat_io = azo_context_register_exception_category_by_str(ctx, (const uint8_t *) "IOException");
	assert(cat_io == AZ_EXCEPTION_CATEGORY_IO);
	uint64_t cat_arithmetic = azo_context_register_exception_category_by_str(ctx, (const uint8_t *) "ArithmeticException");
	/* AZ exceptions */
	unsigned int exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "SystemFailure", cat_system);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "OutOfMemory", cat_system);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "NullDereference", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "InvalidType", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "InvalidValue", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "OutOfBounds", cat_runtime);
	assert(exc == AZ_EXCEPTION_OUT_OF_BOUNDS);
	/* AZO exceptions */
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "ChangeOfMagnitude", cat_arithmetic);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "ChangeOfPrecision", cat_arithmetic);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "NotFinite", cat_arithmetic);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "IntegerOverflow", cat_arithmetic);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "DivideByZero", cat_arithmetic);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "StackOverflow", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "InvalidConversion", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "InvalidProperty", cat_runtime);
	exc = azo_context_register_exception_by_str(ctx, (const uint8_t *) "NotWritable", cat_runtime);
	assert(exc == AZO_EXCEPTION_NOT_WRITABLE);
}

