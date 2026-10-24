#ifndef __AZO_EXCEPTION_H__
#define __AZO_EXCEPTION_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

typedef struct _AZOException AZOException;

#include <az/context.h>

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

	/* Non-maskable exceptions */
	AZO_EXCEPTION_STACK_OVERFLOW,
	/* No conversion is possible */
	AZO_EXCEPTION_INVALID_CONVERSION,
	/* No property with this key */
	AZO_EXCEPTION_INVALID_PROPERTY,
	/* Division by zero */
	AZO_EXCEPTION_DIVIDE_BY_ZERO,
	/* Value is final and cannot be written */
	AZO_EXCEPTION_NOT_WRITABLE,
};

struct _AZOException {
	AZException az_exc;
	/* IPC at the time of exception */
	const uint8_t *ipc;
};

#ifdef __cplusplus
}
#endif

#endif
