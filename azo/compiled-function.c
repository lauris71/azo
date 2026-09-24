#define __AZO_COMPILED_FUNCTION_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define DEBUG_CFUNC 0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <arikkei/arikkei-utils.h>

#include <az/class.h>
#include <az/field.h>
#include <az/packed-value.h>
#include <az/private.h>
#include <az/extend.h>

#include <azo/compiled-function.h>
#include <azo/node.h>

static void aosora_compiled_function_class_init (AZOCompiledFunctionClass *klass);
static void aosora_compiled_function_finalize (AZOCompiledFunctionClass *klass, AZOCompiledFunction *func);

/* AZObject implementation */
static void compiled_function_shutdown (AZObject *obj);
/* AZFunction implementation */
static const AZFunctionSignature *compiled_function_signature(const AZFunctionImplementation *impl, void *inst);
static unsigned int compiled_function_invoke(const AZFunctionImplementation *impl, void *inst, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx);

// Method implementations
static unsigned int compiled_function_call_list (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx);

enum {
	/* Functions */
	FUNC_LIST,
	NUM_PROPERTIES
};

unsigned int
azo_compiled_function_get_type (void)
{
	static unsigned int type = 0;
	if (!type) {
		az_register_type (&type, (const unsigned char *) "AZOCompiledFunction", AZ_TYPE_OBJECT, sizeof (AZOCompiledFunctionClass), sizeof (AZOCompiledFunction), 0, 1, NUM_PROPERTIES,
			(void (*) (AZClass *)) aosora_compiled_function_class_init,
			NULL,
			(void (*) (const AZImplementation *, void *)) aosora_compiled_function_finalize);
	}
	return type;
}

static void
aosora_compiled_function_class_init (AZOCompiledFunctionClass *klass)
{
	az_class_declare_interface ((AZClass *) klass, 0, AZ_TYPE_FUNCTION, ARIKKEI_OFFSET (AZOCompiledFunctionClass, function_impl), 0);
	az_class_define_method_va ((AZClass *) klass, FUNC_LIST, (const unsigned char *) "list", compiled_function_call_list, AZ_TYPE_NONE, 0);
	/* Implementation */
	klass->object_class.shutdown = compiled_function_shutdown;
	klass->function_impl.signature = compiled_function_signature;
	klass->function_impl.invoke = compiled_function_invoke;
}

static void
aosora_compiled_function_finalize (AZOCompiledFunctionClass *klass, AZOCompiledFunction *cfunc)
{
	az_function_signature_delete(cfunc->signature);
}

static void
compiled_function_shutdown (AZObject *obj)
{
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) obj;
	if (cfunc->root) {
		azo_node_free_tree (cfunc->root);
		cfunc->root = NULL;
	}
	if (cfunc->prog) {
		azo_program_unref(cfunc->prog);
		cfunc->prog = NULL;
	}
	azo_datablock_clear(&cfunc->static_data);
}

static unsigned int
compiled_function_call_list (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx)
{
	AZOCompiledFunction *func = (AZOCompiledFunction *) arg_vals[0]->reference;
	if (func->prog) {
		azo_program_print_bytecode (func->prog);
	} else {
		fprintf (stdout, "NO CODE");
	}
	return 1;
}

static const
AZFunctionSignature *compiled_function_signature(const AZFunctionImplementation *impl, void *inst)
{
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) inst;
	return cfunc->signature;
}

static unsigned int
compiled_function_invoke (const AZFunctionImplementation *impl, void *inst, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx)
{
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) inst;

	/* We have to keep reference during invocation */
	az_object_ref ((AZObject *) cfunc);

	azo_program_interpret(cfunc->prog, cfunc->ctx->intr, cfunc->signature->n_args, arg_impls, arg_vals, ret_impl, &ret_val->value, 64);

	az_object_unref ((AZObject *) cfunc);

	return 1;
}

AZOCompiledFunction *
azo_compiled_function_new(AZOProgram *prog)
{
	/* fixme: Implement subprograms as program values */
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) az_object_new (AZO_TYPE_COMPILED_FUNCTION);
	cfunc->ctx = prog->ctx;

	cfunc->prog = prog;
	azo_program_ref(prog);
	cfunc->signature = az_function_signature_new_any(AZ_TYPE_ANY, prog->ret_type, prog->n_args);

	azo_datablock_init(&cfunc->static_data, prog->n_const, prog->n_shared);
	return cfunc;
}

#define noDEBUG_BIND

void
azo_compiled_function_bind (AZOCompiledFunction *cfunc, unsigned int pos, const AZImplementation *impl, void *inst)
{
#ifdef DEBUG_BIND
	unsigned char d[256];
	unsigned int len = az_instance_to_string (impl, inst, d, 255);
	d[len] = 0;
	fprintf (stderr, "azo_compiled_function_bind: Binding %s to pos %u\n", d, pos);
#endif
	azo_datablock_set(&cfunc->prog->shared_data, pos, impl, inst, 0);
}
