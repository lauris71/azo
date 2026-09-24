#define __AZO_CONSOLE_C__

#include <az/az.h>
#include <azo/context.h>
#include <azo/source.h>
#include <azo/parser.h>
#include <azo/node.h>
#include <azo/compiler/compiler.h>
#include <azo/compiler/optimizer.h>

#include <stdio.h>
#include <string.h>

static const char *compile_src = ""
"int32 a = 1, b = 2, c = 3 + (4 + 5i);\n"
"a = b + 4;\n"
"b = a + (b + c) + 5;\n"
"";

int
main(int argc, const char *argv[])
{
    az_init();
    AZOSource *src = azo_source_new_static((const uint8_t *) "test-source", (const uint8_t *) compile_src, strlen(compile_src));
    AZOParser parser;
    azo_parser_setup(&parser, src);
    fprintf(stderr, "-------- PARSING --------\n");
    AZONode *expr = azo_parser_parse(&parser);
    if (!expr) {
        fprintf(stderr, "Parse error\n");
        return 1;
    }
    azo_node_print_info(expr, stdout, src, 0);

    fprintf(stderr, "-------- RESOLVING --------\n");
    AZOContext *ctx = azo_context_new();
    azo_context_define_basic_types(ctx);
    AZOCompilerContext comp_ctx = {
		.globals = ctx,
	};
	AZOCompiler comp;
	azo_compiler_setup(&comp, &comp_ctx, src);
	azo_compiler_push_frame(&comp, NULL, NULL, 0, AZ_TYPE_NONE);
	int result = azo_compiler_resolve_frame(&comp, expr);
    azo_node_print_info(expr, stdout, src, 0);

    fprintf(stderr, "-------- OPTIMIZING --------\n");
    AZOOptimizer opt;
	azo_optimizer_setup(&opt, &comp);
	result = azo_compiler_optimize(&opt, expr, AZO_OPTIMIZER_FLAG_ALL);
    azo_node_print_info(expr, stdout, src, 0);

    azo_optimizer_release(&opt);

    return 0;
}