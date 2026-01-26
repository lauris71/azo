#define __AZO_EXCEPTION_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2018
*/

#include <stdlib.h>
#include <string.h>

#include <arikkei/arikkei-strlib.h>
#include <az/extend.h>

#include "exception.h"

static void exception_class_init (AZOExceptionClass *klass);
/* AZClass implementation */
static unsigned int exception_to_string (const AZImplementation* impl, void *instance, unsigned char *buf, unsigned int len);

unsigned int azo_exception_type = 0;
AZOExceptionClass *azo_exception_class = NULL;

unsigned int
azo_exception_get_type (void)
{
	if (!azo_exception_type) {
		az_register_type (&azo_exception_type, (const unsigned char *) "AZOException", AZ_TYPE_STRUCT, sizeof (AZOExceptionClass), sizeof (AZOException), 0,
			(void (*) (AZClass *)) exception_class_init,
			NULL, NULL);
		azo_exception_class = (AZOExceptionClass *) az_type_get_class (azo_exception_type);
	}
	return azo_exception_type;
}

static void
exception_class_init (AZOExceptionClass *klass)
{
	klass->klass.to_string = exception_to_string;
}

static const char *excnames[] = {
	NULL,
	"System failure",
	"Stack overflow",
	"Invalid type",
	"Invalid conversion",
	"Change of value",
	"Loss of precision",
	"NULL dereference",
	"Out of bounds",
	"Invalid property"
};

unsigned int
static exception_to_string (const AZImplementation* impl, void *inst, unsigned char *buf, unsigned int len)
{
	AZOException *exc = (AZOException *) inst;
	return arikkei_strncpy (buf, len, (const unsigned char *) excnames[exc->type]);
}

void
azo_exception_set (AZOException *exc, unsigned int type, unsigned int mask, unsigned int ipc)
{
 	exc->type = type;
	exc->mask = mask;
	exc->ipc = ipc;
}

void
azo_exception_clear (AZOException *exc)
{
	memset (exc, 0, sizeof (AZOException));
}
