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
	/* Type -> registration lookup (types are allocated sequentially, no holes) */
	AZOExceptionInfo *exc_types;
	unsigned int n_exc_types;
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
	azo_context_clear_exception(ctx);
	for (unsigned int i = 0; i < ctx->priv->n_exc_types; i++) {
		if (ctx->priv->exc_types[i].name) az_string_unref(ctx->priv->exc_types[i].name);
	}
	free(ctx->priv->exc_types);
	arikkei_dict_release (&ctx->priv->exc_defs);
	arikkei_dict_release (&ctx->priv->definitions);
	free(ctx->priv);

	free(ctx);
}

static uint64_t
exc_get_mask (AZOContext *ctx, unsigned int type)
{
	const AZOExceptionInfo *info = azo_context_get_exception_info(ctx, type);
	uint64_t mask = (info) ? info->category : az_get_exception_category(type);
	if (!mask) mask = AZ_EXCEPTION_CATEGORY_SYSTEM;
	return mask;
}

void
azo_context_set_exception(AZOContext *ctx, unsigned int type, const uint8_t *msg, const uint8_t *ipc)
{
	if (!msg) {
		AZString *name = azo_context_get_exception_name(ctx, type);
		msg = (name) ? (const uint8_t *) name->str : (const uint8_t *) "Unknown exception";
	}
	az_exception_set(&ctx->az_ctx.exc, type, exc_get_mask(ctx, type), msg);
	ctx->az_ctx.exc.level = AZO_EXCEPTION_LEVEL;
	ctx->exc_ip = ipc;
}

void
azo_context_set_exception_value(AZOContext *ctx, unsigned int type, AZString *msg, const AZPackedValue *payload, const uint8_t *ipc)
{
	assert(!ctx->az_ctx.exc.type);
	ctx->az_ctx.exc.level = AZO_EXCEPTION_LEVEL;
	ctx->az_ctx.exc.type = type;
	ctx->az_ctx.exc.mask = exc_get_mask(ctx, type);
	if (msg) {
		az_string_ref(msg);
		ctx->az_ctx.exc.msg = msg;
	} else {
		AZString *name = azo_context_get_exception_name(ctx, type);
		ctx->az_ctx.exc.msg = az_string_new((name) ? (const uint8_t *) name->str : (const uint8_t *) "Unknown exception");
	}
	if (payload) az_packed_value_copy(&ctx->exc_payload, payload);
	ctx->exc_ip = ipc;
}

const AZOExceptionInfo *
azo_context_get_exception_info(AZOContext *ctx, unsigned int type)
{
	if (type >= ctx->priv->n_exc_types) return NULL;
	if (!ctx->priv->exc_types[type].name) return NULL;
	return &ctx->priv->exc_types[type];
}

AZString *
azo_context_get_exception_name(AZOContext *ctx, unsigned int type)
{
	const AZOExceptionInfo *info = azo_context_get_exception_info(ctx, type);
	return (info) ? info->name : NULL;
}

void
azo_context_clear_exception(AZOContext *ctx)
{
	az_exception_clear(&ctx->az_ctx.exc);
	ctx->exc_ip = NULL;
	az_packed_value_clear(&ctx->exc_payload);
	for (unsigned int i = 0; i < ctx->n_traces; i++) {
		azo_stack_trace_element_clear(&ctx->traces[i]);
	}
	ctx->n_traces = 0;
	ctx->n_truncated = 0;
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
	/* Types are allocated sequentially so the info array has no holes */
	if (new_def.type >= ctx->priv->n_exc_types) {
		unsigned int n_types = (ctx->priv->n_exc_types) ? ctx->priv->n_exc_types : 16;
		while (new_def.type >= n_types) n_types *= 2;
		ctx->priv->exc_types = (AZOExceptionInfo *) realloc(ctx->priv->exc_types, n_types * sizeof(AZOExceptionInfo));
		memset(ctx->priv->exc_types + ctx->priv->n_exc_types, 0, (n_types - ctx->priv->n_exc_types) * sizeof(AZOExceptionInfo));
		ctx->priv->n_exc_types = n_types;
	}
	az_string_ref(name);
	ctx->priv->exc_types[new_def.type].name = name;
	ctx->priv->exc_types[new_def.type].category = category;
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
	azo_namespace_define_class_by_str(ns, (const unsigned char *) "Exception", AZO_TYPE_EXCEPTION);

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

