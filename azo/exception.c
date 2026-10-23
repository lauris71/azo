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

AZOException *
azo_exception_new()
{
	AZOException *exc = (AZOException *) calloc(1, sizeof(AZOException));
	return exc;
}

void
azo_exception_free(AZOException *exc)
{
	azo_exception_finalize(exc);
	free(exc);
}

void
azo_exception_init(AZOException *exc)
{
}

void
azo_exception_finalize(AZOException *exc)
{
	az_exception_clear(&exc->az_exc);
}

static const char *excnames[] = {
	NULL,
	"System failure",
	"Change of magnitude",
	"Change of precision",
	"Non-finite result",
	"Integer overflow",
	"Stack overflow",
	"Invalid type",
	"Invalid conversion",
	"NULL dereference",
	"Out of bounds",
	"Invalid property",
	"Invalid value",
	"Divide by zero",
	"Not writable"
};

unsigned int
azo_exception_to_string (const AZOException *exc, unsigned char *buf, unsigned int len)
{
	return arikkei_strncpy (buf, len, (const unsigned char *) excnames[exc->az_exc.type]);
}

void
azo_exception_set (AZOException *exc, unsigned int type, unsigned int mask, const uint8_t *ipc)
{
 	exc->az_exc.type = type;
	exc->az_exc.mask = mask;
	exc->ipc = ipc;
}

void
azo_exception_clear (AZOException *exc)
{
	memset (exc, 0, sizeof (AZOException));
}
