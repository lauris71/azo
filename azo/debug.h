#ifndef __AZO_DEBUG_H__
#define __AZO_DEBUG_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <azo/expression.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZODebugTerm AZODebugTerm;
typedef struct _AZODebugInfo AZODebugInfo;

struct _AZODebugTerm {
    AZOTerm term;
    unsigned int tc_start;
    unsigned int tc_end;
};

struct _AZODebugInfo {
    unsigned int n_terms;
    AZODebugTerm *terms;
    unsigned int terms_size;
};

void azo_debug_info_release(AZODebugInfo *dbg);

#ifdef __cplusplus
}
#endif

#endif
