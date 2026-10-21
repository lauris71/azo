#ifndef __AZO_EXCEPTION_H__
#define __AZO_EXCEPTION_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016-2018
*/

#define AZO_TYPE_EXCEPTION ((azo_exception_type) ? azo_exception_type : azo_exception_get_type ())

typedef struct _AZOException AZOException;
typedef struct _AZOExceptionClass AZOExceptionClass;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __AZO_EXCEPTION_C__
extern unsigned int azo_exception_type;
extern AZOExceptionClass *azo_exception_class;
#endif

#define AZO_EXCEPTION_NONE 0

/* System exception - unrecoverable error in either program or interpreter */
#define AZO_EXCEPTION_SYSTEM 1
#define AZO_EXCEPTION_INVALID_OPCODE AZO_EXCEPTION_SYSTEM
#define AZO_EXCEPTION_STACK_UNDERFLOW AZO_EXCEPTION_SYSTEM
#define AZO_EXCEPTION_PREMAURE_END AZO_EXCEPTION_SYSTEM

/* Maskable exceptions - contiguous values starting from 2, the mask bit is
 * 1U << value. A set bit in an exception mask suppresses the exception. */
/* Conversion changes magnitude (clamps) */
#define AZO_EXCEPTION_CHANGE_OF_MAGNITUDE 2
/* Conversion changes precision (rounds) */
#define AZO_EXCEPTION_CHANGE_OF_PRECISION 3
/* Non-finite floating-point result */
#define AZO_EXCEPTION_NOT_FINITE 4
/* Integer overflow */
#define AZO_EXCEPTION_INTEGER_OVERFLOW 5

/* Non-maskable exceptions */
#define AZO_EXCEPTION_STACK_OVERFLOW 6
/* Invalid type for given operation */
#define AZO_EXCEPTION_INVALID_TYPE 7
/* No conversion is possible */
#define AZO_EXCEPTION_INVALID_CONVERSION 8
/* NULL dereference */
#define AZO_EXCEPTION_NULL_DEREFERENCE 9
/* Array reference out of bounds */
#define AZO_EXCEPTION_OUT_OF_BOUNDS 10
/* No property with this key */
#define AZO_EXCEPTION_INVALID_PROPERTY 11
/* Cannot write value */
#define AZO_EXCEPTION_INVALID_VALUE 12
/* Division by zero */
#define AZO_EXCEPTION_DIVIDE_BY_ZERO 13
/* Value is final and cannot be written */
#define AZO_EXCEPTION_NOT_WRITABLE 14

/* Mask bit for an exception (maskable exceptions are at contiguous values) */
#define AZO_EXCEPTION_MASK_BIT(t) (1U << (t))

/* The four maskable exceptions occupy bits 2..5 (0x3c) */
#define AZO_EXCEPTION_MASKABLE \
	(AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_CHANGE_OF_MAGNITUDE) | \
	 AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_CHANGE_OF_PRECISION) | \
	 AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_NOT_FINITE) | \
	 AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_INTEGER_OVERFLOW))

/* Exceptions masked (suppressed) by default in the computation mode */
#define AZO_EXCEPTION_MASK_DEFAULT \
	(AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_CHANGE_OF_PRECISION) | AZO_EXCEPTION_MASK_BIT (AZO_EXCEPTION_NOT_FINITE))

/* Whether an exception is masked (suppressed) in the given flag mask */
#define AZO_EXCEPTION_IS_MASKED(flags, type) \
	((flags) & AZO_EXCEPTION_MASKABLE & AZO_EXCEPTION_MASK_BIT (type))

struct _AZOException {
	unsigned int type;
	unsigned int mask;
	/* IPC at the time of exception */
	const uint8_t *ipc;
};

struct _AZOExceptionClass {
	AZClass klass;
};

unsigned int azo_exception_get_type (void);

void azo_exception_set (AZOException *exc, unsigned int type, unsigned int mask, const uint8_t *ipc);
void azo_exception_clear (AZOException *exc);

#ifdef __cplusplus
}
#endif

#endif
