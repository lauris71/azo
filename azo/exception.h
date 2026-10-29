#ifndef __AZO_EXCEPTION_H__
#define __AZO_EXCEPTION_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#define AZO_TYPE_CATCH_HANDLE azo_catch_handle_get_type()
#define AZO_TYPE_STACK_TRACE azo_stack_trace_get_type()
#define AZO_TYPE_EXCEPTION azo_exception_get_type()

typedef struct _AZOContext AZOContext;

typedef struct _AZOCatchHandle AZOCatchHandle;
typedef struct _AZOStackTraceElement AZOStackTraceElement;
typedef struct _AZOStackTrace AZOStackTrace;
typedef struct _AZOException AZOException;

#include <az/types.h>
#include <az/reference.h>
#include <az/function-value.h>
#include <azo/program.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZO_EXCEPTION_LEVEL (AZ_EXCEPTION_LEVEL + 1)

/* System exception - unrecoverable error in either program or interpreter */
#define AZO_EXCEPTION_INVALID_OPCODE AZ_EXCEPTION_SYSTEM
#define AZO_EXCEPTION_STACK_UNDERFLOW AZ_EXCEPTION_SYSTEM
#define AZO_EXCEPTION_PREMAURE_END AZ_EXCEPTION_SYSTEM

enum AZOExceptionTypes {
	/* Maskable exceptions */
	/* Conversion changes magnitude (clamps) */
	AZO_EXCEPTION_CHANGE_OF_MAGNITUDE = AZ_NUM_EXCEPTIONS,
	/* Conversion changes precision (rounds) */
	AZO_EXCEPTION_CHANGE_OF_PRECISION,
	/* Non-finite floating-point result */
	AZO_EXCEPTION_NOT_FINITE,
	/* Integer overflow */
	AZO_EXCEPTION_INTEGER_OVERFLOW,
	/* Division by zero */
	AZO_EXCEPTION_DIVIDE_BY_ZERO,

	/* Non-maskable exceptions */
	AZO_EXCEPTION_STACK_OVERFLOW,
	/* No conversion is possible */
	AZO_EXCEPTION_INVALID_CONVERSION,
	/* No property with this key */
	AZO_EXCEPTION_INVALID_PROPERTY,
	/* Value is final and cannot be written */
	AZO_EXCEPTION_NOT_WRITABLE,
};

struct _AZOCatchHandle {
	/* Ip of catch block */
	uint32_t ip;
	/* Type of exception to catch */
	uint32_t type;
	/* Cathegory mask */
	uint64_t mask;
};

unsigned int azo_catch_handle_get_type();

struct _AZOStackTraceElement {
	AZOProgram *prog;
	unsigned int ip;
};

void azo_stack_trace_element_set(AZOStackTraceElement *element, AZOProgram *prog, unsigned int ip);
void azo_stack_trace_element_clear(AZOStackTraceElement *element);

struct _AZOStackTrace {
	AZReference reference;
	unsigned int n_elements;
	unsigned int n_truncated;
	AZOStackTraceElement elements[];
};

unsigned int azo_stack_trace_get_type();

/**
 * @brief Create a new stack trace from the given context
 * 
 * Does not clear exception formation from context
 * 
 * @param ctx 
 * @return AZOStackTrace* 
 */
AZOStackTrace *azo_stack_trace_new(AZOContext *ctx);
static inline void
azo_stack_trace_unref(AZOStackTrace *trace)
{
	az_reference_unref((AZReferenceClass *) AZ_CLASS_FROM_TYPE(AZO_TYPE_STACK_TRACE), &trace->reference);
}

struct _AZOException {
	AZReference reference;
	AZException az_exc;
	/* Throw site */
	const uint8_t *ip;
	AZPackedValue payload;
	AZOStackTrace *stack_trace;
};

unsigned int azo_exception_get_type();

/**
 * @brief Materialize a new exception value from the context exception state
 *
 * Consumes (clears) the context exception state.
 */
AZOException *azo_exception_new(AZOContext *ctx);

/**
 * @brief Set the context exception state from an exception value (throw e)
 *
 * Seeds the context trace accumulator from the value's stack trace, so a
 * rethrown value keeps its original throw site and traceback. The context
 * exception state must be clear.
 */
void azo_exception_set_context(AZOContext *ctx, const AZOException *exception);

/* Static void (AZOException) function value that throws the exception value
 * when invoked - used to compile "throw e" without a dedicated opcode */
const AZFunctionValue *azo_exception_get_throw_function (void);

static inline void
azo_exception_unref(AZOException *exception)
{
	az_reference_unref((AZReferenceClass *) AZ_CLASS_FROM_TYPE(AZO_TYPE_EXCEPTION), &exception->reference);
}

#ifdef __cplusplus
}
#endif

#endif
