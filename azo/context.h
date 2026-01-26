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

#define AZO_TYPE_CONTEXT azo_context_get_type ()
#define AZO_CONTEXT(o) (AZ_CHECK_INSTANCE_CAST ((o), AZO_TYPE_CONTEXT, AZOContext))
#define AZO_IS_CONTEXT(o) (AZ_CHECK_INSTANCE_TYPE ((o), AZO_TYPE_CONTEXT))

typedef struct _AZOContext AZOContext;
typedef struct _AZOContextClass AZOContextClass;

#include <az/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZOContext {
	AZContext *az_ctx;
	AZOInterpreter *intr;
};

AZOContext *azo_context_new (void);
void azo_context_delete (AZOContext *ctx);
unsigned int azo_context_define (AZOContext *ctx, AZString *key, const AZPackedValue *value);
unsigned int azo_context_define_by_str (AZOContext *ctx, const unsigned char *key, const AZPackedValue *value);
unsigned int azo_context_lookup (AZOContext *ctx, AZString *key, AZPackedValue *value);
unsigned int azo_context_lookup_by_str (AZOContext *ctx, const unsigned char *key, AZPackedValue *value);

void azo_context_define_basic_types (AZOContext *ctx);
unsigned int azo_context_define_class_by_str (AZOContext *ctx, const unsigned char *key, unsigned int type);

#ifdef __cplusplus
}
#endif

#endif
