#define __AZO_DEBUG_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2026
*/

#include <azo/code.h>
#include <azo/debug.h>

void
azo_debug_info_setup(AZODebugInfo *dbg, const AZOCode *code, AZOSource *src)
{
    dbg->n_terms = code->bc_len;
    dbg->terms = (AZODebugTerm *) malloc(code->bc_len * sizeof(AZODebugTerm));
    unsigned int line = 0;
    for (unsigned int i = 0; i < code->bc_len; i++) {
        if (code->exprs[i]) {
            dbg->terms[i].term = code->exprs[i]->term;
            azo_source_find_line_range(src, code->exprs[i]->term.start, code->exprs[i]->term.end, &line, NULL);
        } else {
            dbg->terms[i] = (AZODebugTerm) {0};
        }
        dbg->terms[i].line = line;
    }
    dbg->src = src;
    azo_source_ref(dbg->src);
}

void
azo_debug_info_release(AZODebugInfo *dbg)
{
    if (dbg->terms) {
        free(dbg->terms);
        dbg->terms = NULL;
        dbg->n_terms = 0;
    }
    if (dbg->src) azo_source_unref(dbg->src);
}

