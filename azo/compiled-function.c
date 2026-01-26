#define __AZO_COMPILED_FUNCTION_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#define debug 0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <arikkei/arikkei-utils.h>

#include <az/class.h>
#include <az/field.h>
#include <az/packed-value.h>
#include <az/private.h>
#include <az/extend.h>

#include <azo/compiled-function.h>
#include <azo/expression.h>

static void aosora_compiled_function_class_init (AZOCompiledFunctionClass *klass);
static void aosora_compiled_function_finalize (AZOCompiledFunctionClass *klass, AZOCompiledFunction *func);

/* AZObject implementation */
static void compiled_function_shutdown (AZObject *obj);
/* AZFunction implementation */
static const AZFunctionSignature *compiled_function_signature(const AZFunctionImplementation *impl, void *inst);
static unsigned int compiled_function_invoke(const AZFunctionImplementation *impl, void *inst, const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx);

// Method implementations
static unsigned int aosora_compiled_function_call_list (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx);

enum {
	/* Functions */
	FUNC_LIST,
	NUM_FUNCTIONS,
	/* Values */
	PROP_BOUND = NUM_FUNCTIONS,
	NUM_PROPERTIES
};

unsigned int
azo_compiled_function_get_type (void)
{
	static unsigned int type = 0;
	if (!type) {
		az_register_type (&type, (const unsigned char *) "AZOCompiledFunction", AZ_TYPE_OBJECT, sizeof (AZOCompiledFunctionClass), sizeof (AZOCompiledFunction), 0,
			(void (*) (AZClass *)) aosora_compiled_function_class_init,
			NULL,
			(void (*) (const AZImplementation *, void *)) aosora_compiled_function_finalize);
	}
	return type;
}

static void
aosora_compiled_function_class_init (AZOCompiledFunctionClass *klass)
{
	/* Interfaces */
	az_class_set_num_interfaces ((AZClass *) klass, 1);
	az_class_declare_interface ((AZClass *) klass, 0, AZ_TYPE_FUNCTION, ARIKKEI_OFFSET (AZOCompiledFunctionClass, function_impl), 0);
	az_class_set_num_properties ((AZClass *) klass, NUM_PROPERTIES);
	az_class_define_method_va ((AZClass *) klass, FUNC_LIST, (const unsigned char *) "list", aosora_compiled_function_call_list, AZ_TYPE_NONE, 0);
	az_class_define_property ((AZClass *) klass, PROP_BOUND, (const unsigned char *) "bound", AZ_TYPE_BOOLEAN, 0, 
		AZ_FIELD_INSTANCE, AZ_FIELD_READ_VALUE, AZ_FIELD_WRITE_NONE, ARIKKEI_OFFSET(AZOCompiledFunction,bound), NULL, NULL);
	/* Implementation */
	klass->object_class.shutdown = compiled_function_shutdown;
	klass->function_impl.signature = compiled_function_signature;
	klass->function_impl.invoke = compiled_function_invoke;
}

static void
aosora_compiled_function_finalize (AZOCompiledFunctionClass *klass, AZOCompiledFunction *func)
{
	az_function_signature_delete(func->signature);
}

static void
compiled_function_shutdown (AZObject *obj)
{
	AZOCompiledFunction *cfunc = (AZOCompiledFunction *) obj;
	if (cfunc->root) {
		azo_expression_free_tree (cfunc->root);
		cfunc->root = NULL;
	}
	if (cfunc->prog) {
		azo_program_delete (cfunc->prog);
		cfunc->prog = NULL;
	}
}

static unsigned int
aosora_compiled_function_call_list (const AZImplementation **arg_impls, const AZValue **arg_vals, const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx)
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

	ARIKKEI_CHECK_INTEGRITY ();
	/* We have to keep reference during invocation */
	az_object_ref ((AZObject *) cfunc);

	unsigned int frame = cfunc->ctx->intr->n_frames;
	azo_interpreter_push_frame (cfunc->ctx->intr, 0);
	azo_intepreter_push_values (cfunc->ctx->intr, arg_impls, arg_vals, cfunc->signature->n_args);
	interpreter_interpret (cfunc->ctx->intr, cfunc->prog, ret_impl, ret_val);
	azo_interpreter_restore_frame (cfunc->ctx->intr, frame);

	az_object_unref ((AZObject *) cfunc);
	ARIKKEI_CHECK_INTEGRITY ();

	return 1;
}

AZOCompiledFunction *
azo_compiled_function_new (AZOContext *ctx, AZOProgram *program, unsigned int ret_type, unsigned int nargs)
{
	/* fixme: Implement subprograms as program values */
	AZOCompiledFunction *func = (AZOCompiledFunction *) az_object_new (AZO_TYPE_COMPILED_FUNCTION);
	func->ctx = ctx;

	func->prog = program;
	if (debug) {
		fprintf (stderr, "aosora_compiled_function_new\n");
		azo_program_print_bytecode (program);
	}
	/* fixme: Implement return types */
	func->signature = az_function_signature_new_any(AZ_TYPE_ANY, ret_type, nargs);
	return func;
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
	az_packed_value_set_from_impl_instance (&cfunc->prog->values[pos], impl, inst);
	cfunc->bound = 1;
}
