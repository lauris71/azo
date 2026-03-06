#define __AZO_DEBUGGER_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2025
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

unsigned int
azo_debugger_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return (result >= 0) ? result : 0;
}

unsigned int
azo_debugger_get_command(ArikkeiToken *tokens, unsigned tokens_len)
{
    fprintf(stdout, ">");
    if(!tokens_len) return DBG_UNKNOWN;
    uint8_t buf[256];
    unsigned int len = 0;
    while(len < 256) {
        int val = getchar();
        if ((val < 0) || (val == '\n')) break;
        buf[len++] = (uint8_t) val;
    }
    ArikkeiToken token = {buf, len};
    return arikkei_token_tokenize(&token, tokens, tokens_len, 1, 1);
}

static void
print_line(AZODebugger *debugger, AZOProgram *prog, unsigned int line)
{
    azo_source_ensure_lines(prog->debug.src);
    azo_debugger_printf("%4d: ", line);
    if (line >= prog->debug.src->n_lines) {
        azo_debugger_printf("EOF\n");
        return;
    }
    unsigned int s = prog->debug.src->lines[line];
    unsigned int len = azo_source_get_line_len(prog->debug.src, line);
    for (unsigned int j = 0; j < len; j++) {
        azo_debugger_printf("%c", prog->debug.src->cdata[s + j]);
    }
    azo_debugger_printf("\n");
}

void
print_stack (AZODebugger *debugger)
{
    AZOInterpreter *intr = debugger->intr;
    AZOStack *stack = &debugger->intr->stack;
	for (unsigned int i = 0; i < intr->n_frames; i++) {
		unsigned int frame = intr->n_frames - 1 - i;
        unsigned int start = intr->frames[i];
		unsigned int end = (frame == (intr->n_frames - 1)) ? stack->length : intr->frames[frame + 1];
		if (end > intr->frames[frame]) {
			azo_debugger_printf("Frame %u (%u - %u)\n", frame, intr->frames[frame], end - 1);
		} else {
			azo_debugger_printf("Frame %u EMPTY\n", frame);
		}
        for (unsigned int i = intr->frames[i]; i < end; i++) {
            unsigned int pos = end - 1 - (i - start);
            uint8_t buf[1024];
            unsigned int len = azo_stack_print(stack, pos, buf, 1024);
            azo_debugger_printf("%s\n", buf);
        }
	}
}

void
azo_debugger_run(AZODebugger *debugger, AZOProgram *prog)
{
    // fixme: Make standalone
    azo_debugger_printf("\nStarting debugger: %s\n", (prog->debug.src->name) ? (const char *) prog->debug.src->name->str : "unnamed");

    unsigned int ip = 0;

    while(ip < prog->tcode_length) {
        unsigned int line = prog->debug.terms[ip].line;
        print_line(debugger, prog, line);

        ArikkeiToken tokenz[256];
        unsigned int n_tokenz = azo_debugger_get_command(tokenz, 256);
        if (!n_tokenz) continue;
        if (arikkei_token_is_equal_str(tokenz[0], (const uint8_t *) "q")) {
            break;
        } else if (arikkei_token_is_equal_str(tokenz[0], (const uint8_t *) "l")) {
            // list
            uint64_t n_lines = 5;
            if (n_tokenz >= 2) {
                if (arikkei_strtoull(tokenz[1].cdata, tokenz[1].len, &n_lines) != tokenz[1].len) {
                    azo_debugger_printf("Usage: l [NUM LINES]\n");
                    continue;
                }
            }
            for (unsigned int i = 0; i < n_lines; i++) {
                print_line(debugger, prog, line + i);
            }
            azo_debugger_printf("\n");
        } else if (arikkei_token_is_equal_str(tokenz[0], (const uint8_t *) "lb")) {
            // list
            uint64_t n_lines = 5;
            if (n_tokenz >= 2) {
                if (arikkei_strtoull(tokenz[1].cdata, tokenz[1].len, &n_lines) != tokenz[1].len) {
                    azo_debugger_printf("Usage: l [NUM LINES]\n");
                    continue;
                }
            }
            unsigned int ic = ip;
            for (unsigned int i = 0; i < n_lines; i++) {
                uint8_t buf[1024];
                azo_bc_print_instruction(buf, 1024, prog->tcode, ic, prog->tcode_length);
                azo_debugger_printf("%4d %s\n", ic, buf);
                ic = azo_bc_next_instruction(prog->tcode, ic, prog->tcode_length);
                if (ic >= prog->tcode_length) break;
            }
            azo_debugger_printf("\n");
        } else if (arikkei_token_is_equal_str(tokenz[0], (const uint8_t *) "n")) {
            // next
            while((ip < prog->tcode_length) && (prog->debug.terms[ip].line == line)) {
                const uint8_t *ipc = prog->tcode + ip;
                ipc = azo_interpreter_interpret_tc(debugger->intr, prog, ipc);
                if (!ipc) break;
                ip = ipc - prog->tcode;
            }
            azo_debugger_printf("\n");
        } else if (arikkei_token_is_equal_str(tokenz[0], (const uint8_t *) "bt")) {
            // backtrace
            print_stack(debugger);
            azo_debugger_printf("\n");
        }
    }
}


