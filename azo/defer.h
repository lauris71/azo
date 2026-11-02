#ifndef __AZO_DEFER_H__
#define __AZO_DEFER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define AZO_TYPE_DEFER_HANDLE azo_defer_handle_get_type ()

typedef struct _AZODeferHandle AZODeferHandle;
typedef struct _AZClass AZODeferHandleClass;

#include <az/az.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AZODeferHandle {
    union {
	/* The instruction pointer (defer block) */
	unsigned int ip;
	/* Saved interpreter flags (flags handle) */
	unsigned int flags;
    };
    /* The length of block code; 0 marks a flags handle (nothing to execute) */
    unsigned int len;
};

unsigned int azo_defer_handle_get_type (void);

extern AZODeferHandleClass *azo_defer_handle_class;

#ifdef __cplusplus
};
#endif

#endif

