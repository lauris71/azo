#ifndef __AZO_DEBUG_H__
#define __AZO_DEBUG_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2026
*/

#include <azo/node.h>
#include <azo/code.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AZODebugTerm AZODebugTerm;
typedef struct _AZODebugInfo AZODebugInfo;

struct _AZODebugTerm {
    AZOTerm term;
    unsigned int line;
};

struct _AZODebugInfo {
    unsigned int n_terms;
    AZODebugTerm *terms;
    AZOSource *src;
};

void azo_debug_info_setup(AZODebugInfo *dbg, const AZOCode *code, AZOSource *src);
void azo_debug_info_release(AZODebugInfo *dbg);

void azo_debug_print_term(AZODebugInfo *dbg, unsigned int term_idx, FILE *ofs);

#ifdef __cplusplus
}
#endif

#endif
