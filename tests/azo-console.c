#define __AZO_CONSOLE_C__

#include <az/az.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/parser.h>
#include <azo/node.h>
#include <stdio.h>
#include <string.h>

static const char *compile_src = ""
"itemize = alpha[1].init(\"itemize\");\n"
"";

int
main(int argc, const char *argv[])
{
    az_init();
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) compile_src, strlen(compile_src));
    AZOParser parser;
    azo_parser_setup(&parser, src);
    AZONode *expr = azo_parser_parse(&parser);
    if (!expr) {
        fprintf(stderr, "Parse error\n");
        return 1;
    }
    azo_node_print_info(expr, stdout, src, 0);

    return 0;
}