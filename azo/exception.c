#define __AZO_EXCEPTION_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2018-2026
*/

#include <assert.h>

#include <az/extend.h>
#include <az/string.h>

#include <azo/context.h>
#include <azo/exception.h>

typedef struct _AZClass AZOCatchHandleClass;

unsigned int
azo_catch_handle_get_type()
{
	static unsigned int type = 0;
	unsigned int t = AZ_TYPE_READ(type);
	if (t) return t;
	AZ_TYPES_LOCK();
	if (!type) {
		az_register_type(&type, (const uint8_t *) "AZOCatchHandle", AZ_TYPE_STRUCT, sizeof(AZOCatchHandleClass), sizeof(AZOCatchHandle), AZ_FLAG_FINAL,
			0, 0,
			NULL, NULL, NULL);
	}
	t = type;
	AZ_TYPES_UNLOCK();
	return t;
}

void
azo_stack_trace_element_set(AZOStackTraceElement *element, AZOProgram *prog, unsigned int ip)
{
    assert(prog);
    assert(!element->prog);
    element->prog = prog;
    az_reference_ref((AZReference *) prog);
    element->ip = ip;
}

void
azo_stack_trace_element_clear(AZOStackTraceElement *element)
{
    if (element->prog) {
        azo_program_unref(element->prog);
        element->prog = NULL;
    }
    element->ip = 0;
}

typedef struct _AZReferenceClass AZOStackTraceClass;

enum {
    TRACE_LENGTH,
    TRACE_N_TRUNCATED,
    TRACE_GET_STRING,
    TRACE_N_PROPS
};

static AZString *
trace_get_string(AZOStackTrace *trace, unsigned int idx)
{
    char c[32];
    snprintf(c, sizeof(c), "Unknown file: ip = %u", trace->elements[idx].ip);
    return az_string_new((const uint8_t *) c);
}

static void
trace_class_init(AZOStackTraceClass *klass)
{
    az_class_define_property_value((AZClass *) klass, TRACE_LENGTH, (const uint8_t *) "size", AZ_TYPE_UINT32, 1,
     AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_NONE, offsetof(AZOStackTrace, n_elements));
    az_class_define_property_value((AZClass *) klass, TRACE_N_TRUNCATED, (const uint8_t *) "numTruncated", AZ_TYPE_UINT32, 1,
     AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_NONE, offsetof(AZOStackTrace, n_truncated));
    az_class_define_method_native_va((AZClass *) klass, TRACE_GET_STRING, (const uint8_t *) "getString",
    (void (*) ()) trace_get_string,
    AZ_TYPE_STRING, 1, AZ_TYPE_UINT32);
}

static void
trace_finalize(AZOStackTraceClass *klass, AZOStackTrace *trace)
{
    for (unsigned int i = 0; i < trace->n_elements; i++) {
        azo_stack_trace_element_clear(&trace->elements[i]);
    }
}

unsigned int
azo_stack_trace_get_type()
{
	static unsigned int type = 0;
	unsigned int t = AZ_TYPE_READ(type);
	if (t) return t;
	AZ_TYPES_LOCK();
	if (!type) {
		az_register_type(&type, (const uint8_t *) "AZOStackTrace", AZ_TYPE_REFERENCE, sizeof(AZOStackTraceClass), sizeof(AZOStackTrace), AZ_FLAG_ZERO_MEMORY | AZ_FLAG_FINAL | AZ_FLAG_VARIABLE_SIZE,
			0, TRACE_N_PROPS,
			(void (*) (AZClass *)) trace_class_init,
            NULL,
            (void (*) (const AZImplementation *, void *)) trace_finalize);
	}
	t = type;
	AZ_TYPES_UNLOCK();
	return t;
}

AZOStackTrace *
azo_stack_trace_new(AZOContext *ctx)
{
    AZOStackTrace *trace = (AZOStackTrace *) az_instance_new_sized(AZO_TYPE_STACK_TRACE, sizeof(AZOStackTrace) + ctx->n_traces * sizeof(AZOStackTraceElement));
    trace->n_elements = ctx->n_traces;
    trace->n_truncated = ctx->n_truncated;
    for (unsigned int i = 0; i < ctx->n_traces; i++) {
        azo_stack_trace_element_set(&trace->elements[i], ctx->traces[i].prog, ctx->traces[i].ip);
    }
    return trace;
}

typedef struct _AZReferenceClass AZOExceptionClass;

enum {
    EXCEPTION_TYPE,
    EXCEPTION_CATEGORY,
    EXCEPTION_STACK_TRACE,
    EXCEPTION_N_PROPS
};

static void 
exception_finalize(AZOExceptionClass *klass, AZOException *exception)
{
    az_exception_clear(&exception->az_exc);
    az_packed_value_clear(&exception->payload);
    if (exception->stack_trace) {
        azo_stack_trace_unref(exception->stack_trace);
    }
}

static void
exception_class_init(AZOExceptionClass *klass)
{
    az_class_define_property_value((AZClass *) klass, EXCEPTION_TYPE, (const uint8_t *) "type", AZ_TYPE_UINT32, 1,
     AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_NONE, offsetof(AZOException, az_exc.type));
    az_class_define_property_value((AZClass *) klass, EXCEPTION_CATEGORY, (const uint8_t *) "category", AZ_TYPE_UINT64, 1,
     AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_NONE, offsetof(AZOException, az_exc.mask));
    az_class_define_property_value((AZClass *) klass, EXCEPTION_STACK_TRACE, (const uint8_t *) "stackTrace", AZO_TYPE_STACK_TRACE, 1,
     AZ_FIELD_INSTANCE, AZ_FIELD_WRITE_NONE, offsetof(AZOException, stack_trace));
}

unsigned int
azo_exception_get_type()
{
	static unsigned int type = 0;
	unsigned int t = AZ_TYPE_READ(type);
	if (t) return t;
	AZ_TYPES_LOCK();
	if (!type) {
		az_register_type(&type, (const uint8_t *) "AZOException", AZ_TYPE_REFERENCE, sizeof(AZOExceptionClass), sizeof(AZOException), AZ_FLAG_ZERO_MEMORY | AZ_FLAG_FINAL,
			0, EXCEPTION_N_PROPS,
			(void (*) (AZClass *)) exception_class_init,
            NULL,
            (void (*) (const AZImplementation *, void *)) exception_finalize);
	}
	t = type;
	AZ_TYPES_UNLOCK();
	return t;
}

AZOException *
azo_exception_new(AZOContext *ctx)
{
    AZOException *exc = az_instance_new(AZO_TYPE_EXCEPTION);
    /* Transfer ownership of the context state to the new value */
    exc->az_exc = ctx->az_ctx.exc;
    ctx->az_ctx.exc.msg = NULL;
    exc->ip = ctx->exc_ip;
    exc->payload = ctx->exc_payload;
    ctx->exc_payload.impl = NULL;
    exc->stack_trace = azo_stack_trace_new(ctx);
    azo_context_clear_exception(ctx);
    return exc;
}

void
azo_exception_set_context(AZOContext *ctx, const AZOException *exception)
{
    assert(!ctx->az_ctx.exc.type);
    ctx->az_ctx.exc = exception->az_exc;
    if (ctx->az_ctx.exc.msg) az_string_ref(ctx->az_ctx.exc.msg);
    az_packed_value_copy(&ctx->exc_payload, &exception->payload);
    ctx->exc_ip = exception->ip;
    if (exception->stack_trace) {
        const AZOStackTrace *trace = exception->stack_trace;
        assert(trace->n_elements <= 64);
        for (unsigned int i = 0; i < trace->n_elements; i++) {
            azo_stack_trace_element_set(&ctx->traces[i], trace->elements[i].prog, trace->elements[i].ip);
        }
        ctx->n_traces = trace->n_elements;
        ctx->n_truncated = trace->n_truncated;
    }
}

/* throw e: deconstructs the value into the context exception state; the
 * interpreter unwinds on the pending exception after the invoke returns */
static unsigned int
exception_throw_invoke (const AZImplementation *arg_impls[], const AZValue *arg_vals[], const AZImplementation **ret_impl, AZValue64 *ret_val, AZContext *ctx)
{
    azo_exception_set_context ((AZOContext *) ctx, (AZOException *) arg_vals[0]->reference);
    return 1;
}

static AZFunctionSignature32 exception_throw_signature;
static AZFunctionValue exception_throw_fval;

const AZFunctionValue *
azo_exception_get_throw_function (void)
{
    static unsigned int initialized = 0;
    unsigned int t = AZ_TYPE_READ(initialized);
    if (t) return &exception_throw_fval;
    AZ_TYPES_LOCK();
    if (!initialized) {
        exception_throw_signature.ret_type = AZ_TYPE_NONE;
        exception_throw_signature.n_args = 1;
        exception_throw_signature.arg_types[0] = azo_exception_get_type();
        az_function_value_setup(&exception_throw_fval, &exception_throw_signature.signature, exception_throw_invoke);
        initialized = 1;
    }
    AZ_TYPES_UNLOCK();
    return &exception_throw_fval;
}
