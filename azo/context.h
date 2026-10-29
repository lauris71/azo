#ifndef __AZO_EXECUTION_CONTEXT_H__
#define __AZO_EXECUTION_CONTEXT_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

/*
* A global context for executing code
*
* You should never redefine global variables as these are treated as final by optimizer
*
*/

typedef struct _AZOInterpreter AZOInterpreter;

typedef struct _AZOContext AZOContext;
typedef struct _AZOContextPriv AZOContextPriv;
typedef struct _AZOExceptionDef AZOExceptionDef;

#include <az/context.h>

#include <azo/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOExceptionDef {
	/* AZ_EXCEPTION_NONE means it is a category */
	uint32_t type;
	uint32_t filler;
	uint64_t category;
};

/* Type -> registration lookup (types are allocated sequentially, no holes) */
typedef struct _AZOExceptionInfo {
	AZString *name;
	uint64_t category;
} AZOExceptionInfo;

struct _AZOContext {
	AZContext az_ctx;
	/* Exception ip */
	const uint8_t *exc_ip;
	AZPackedValue exc_payload;
	unsigned int n_traces;
	unsigned int n_truncated;
	AZOStackTraceElement traces[64];
	AZOInterpreter *intr;
	AZOContextPriv *priv;
};

AZOContext *azo_context_new(void);
void azo_context_delete(AZOContext *ctx);

/* The category mask is looked up from the exception registry */
void azo_context_set_exception(AZOContext *ctx, unsigned int type, const uint8_t *msg, const uint8_t *ipc);
/* msg and payload are borrowed (ref'd/copied); msg NULL gives the registered name */
void azo_context_set_exception_value(AZOContext *ctx, unsigned int type, AZString *msg, const AZPackedValue *payload, const uint8_t *ipc);
void azo_context_clear_exception(AZOContext *ctx);

/* Borrowed references, NULL if type is not a registered exception */
const AZOExceptionInfo *azo_context_get_exception_info(AZOContext *ctx, unsigned int type);
AZString *azo_context_get_exception_name(AZOContext *ctx, unsigned int type);

unsigned int azo_context_define (AZOContext *ctx, AZString *key, const AZImplementation *impl, void *inst);
unsigned int azo_context_define_by_str (AZOContext *ctx, const unsigned char *key, const AZImplementation *impl, void *inst);
const AZImplementation *azo_context_lookup (AZOContext *ctx, AZString *key, AZValue *val, unsigned int size);
const AZImplementation *azo_context_lookup_by_str (AZOContext *ctx, const uint8_t *key, AZValue *val, unsigned int size);

unsigned int azo_context_define_class_by_str (AZOContext *ctx, const unsigned char *key, unsigned int type);

/* On error the result is AZ_EXCEPTION_NONE */
unsigned int azo_context_register_exception(AZOContext *ctx, AZString *name, uint64_t category);
/* On error the result is 0 */
uint64_t azo_context_register_exception_category(AZOContext *ctx, AZString *name);
/* Return either exception or category (if type == AZ_EXCEPTION_NONE) */
const AZOExceptionDef *azo_context_lookup_exception(AZOContext *ctx, AZString *name);

void azo_context_define_basic_types (AZOContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
