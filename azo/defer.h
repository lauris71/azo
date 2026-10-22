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
    /* The instruction pointer */
    unsigned int ip;
    /* The length of block code */
    unsigned int len;
};

unsigned int azo_defer_handle_get_type (void);

extern AZODeferHandleClass *azo_defer_handle_class;

#ifdef __cplusplus
};
#endif

#endif

