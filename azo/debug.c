#define __AZO_DEBUG_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2026
*/

#include <azo/debug.h>

void
azo_debug_info_release(AZODebugInfo *dbg)
{
    if (dbg->terms) {
        free(dbg->terms);
        dbg->terms = NULL;
        dbg->n_terms = 0;
        dbg->terms_size = 0;
    }
}

