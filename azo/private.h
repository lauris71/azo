#ifndef __AZO_PRIVATE_H__
#define __AZO_PRIVATE_H__

/*
* A languge implementation based on AZ
*
* Copyright (C) Lauris Kaplinski 2016
*/

#ifdef ARIKKEI_MEMCHECK
#define ARIKKEI_CHECK_INTEGRITY() arikkei_check_integrity()
void arikkei_check_integrity (void);
#else
#define ARIKKEI_CHECK_INTEGRITY()
#endif

#ifdef __cplusplus
}
#endif

#endif /* PRIVATE_H */

