#ifndef __AZO_DEBUGGER_H__
#define __AZO_DEBUGGER_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2025
*/

typedef struct _AZODebugger AZODebugger;

#include <azo/program.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DBG_RUN,
    DBG_STEP_OVER,
    DBG_STEP_INTO,
    DBG_STACK_TRACE
};

struct _AZODebugger {
    AZOInterpreter *intr;
};

AZODebugger *azo_debugger_new(AZOInterpreter *intr);
void azo_debugger_unref(AZODebugger *debugger);

void azo_debugger_run(AZODebugger *debugger, AZOProgram *prog);

#ifdef __cplusplus
}
#endif

#endif
