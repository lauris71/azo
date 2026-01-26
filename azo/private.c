#define __AZO_PRIVATE_C__

#include "private.h"

void
arikkei_check_integrity (void)
{
#ifdef WIN32
    assert (_CrtCheckMemory());
#endif
}
