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

#define AZO_EXCEPTION_STACK_OVERFLOW 2
/* Invalid type for given operation */
#define AZO_EXCEPTION_INVALID_TYPE 3
/* No conversion is possible */
#define AZO_EXCEPTION_INVALID_CONVERSION 4
/* Conversion changes value */
#define AZO_EXCEPTION_CHANGE_OF_MAGNITUDE 5
/* Conversion changes precision */
#define AZO_EXCEPTION_CHANGE_OF_PRECISION 6
/* NULL dereference */
#define AZO_EXCEPTION_NULL_DEREFERENCE 7
/* Array reference out of bounds */
#define AZO_EXCEPTION_OUT_OF_BOUNDS 8
/* No property with this key */
#define AZO_EXCEPTION_INVALID_PROPERTY 9
/* Cannot write value */
#define AZO_EXCEPTION_INVALID_VALUE 10

struct _AZOException {
	unsigned int type;
	unsigned int mask;
	/* IPC at the time of exception */
	unsigned int ipc;
};

struct _AZOExceptionClass {
	AZClass klass;
};

unsigned int azo_exception_get_type (void);

void azo_exception_set (AZOException *exc, unsigned int type, unsigned int mask, unsigned int ipc);
void azo_exception_clear (AZOException *exc);

#ifdef __cplusplus
}
#endif

#endif
