#define __AZO_DEFER_HANDLE_C__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#include <az/extend.h>

#include <azo/defer.h>

AZODeferHandleClass *azo_defer_handle_class = NULL;

unsigned int
azo_defer_handle_get_type (void)
{
	static unsigned int type = 0;
	if (!type) {
		azo_defer_handle_class = (AZODeferHandleClass *) az_register_type (&type, (const uint8_t *) "AZODeferHandle", AZ_TYPE_STRUCT, sizeof (AZODeferHandleClass), sizeof (AZODeferHandle), AZ_FLAG_FINAL, 0, 0,
			NULL, NULL, NULL);
	}
	return type;
}
