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

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOExceptionDef {
	/* AZ_EXCEPTION_NONE means it is a category */
	uint32_t type;
	uint32_t filler;
	uint64_t category;
};

struct _AZOContext {
	AZContext az_ctx;
	/* Exception ip */
	const uint8_t *exc_ip;
	AZOInterpreter *intr;
	AZOContextPriv *priv;
};

AZOContext *azo_context_new(void);
void azo_context_delete(AZOContext *ctx);

void azo_context_set_exception(AZOContext *ctx, unsigned int type, unsigned int mask, const uint8_t *ipc);
void azo_context_clear_exception(AZOContext *ctx);

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
