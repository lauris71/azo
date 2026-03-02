#define __AZO_DEBUGGER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2025
*/

#include <string.h>

#include <az/string.h>

#include "bytecode.h"
#include "debugger.h"
#include "interpreter.h"

AZODebugger *
azo_debugger_new(AZOInterpreter *intr)
{
    AZODebugger *debugger = (AZODebugger *) malloc(sizeof(AZODebugger));
    debugger->intr = intr;
    return debugger;
}

void
azo_debugger_unref(AZODebugger *debugger)
{
    free(debugger);
}

void
azo_debugger_run(AZODebugger *debugger, AZOProgram *prog)
{
    fprintf(stdout, "\nStarting debugger: %s\n", prog->debug.src->name->str);
    for (unsigned int ip = 0; ip < prog->tcode_length; ip = azo_bc_next_instruction(prog->tcode, ip, prog->tcode_length)) {
    uint8_t c[1024];
        azo_bc_print_instruction(c, 1024, prog->tcode, ip, prog->tcode_length);
        fprintf(stdout, "%04d %s\n", ip, c);
    }
    const uint8_t *ipc = prog->tcode;
    const uint8_t *end = prog->tcode + prog->tcode_length;
    while (ipc && (ipc < end)) {
        unsigned int ip = ipc - prog->tcode;
        unsigned int line = prog->debug.terms[ip].line;
        azo_source_print_lines(prog->debug.src, line, line + 1);
        while(ipc && (ipc < end) && (prog->debug.terms[ip].line == line)) {
            ipc = azo_interpreter_interpret_tc(debugger->intr, prog, prog->tcode + ip);
            ip = ipc - prog->tcode;
        }
    }

    azo_interpreter_run(debugger->intr, prog);
}


