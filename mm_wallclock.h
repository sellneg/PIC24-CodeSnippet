/* 
 * File:   mm_wallclock.h
 * Author: mmtools
 *
 * Created on June 8, 2020, 11:40 AM
 */

#ifndef MM_WALLCLOCK_H
#define	MM_WALLCLOCK_H

#include "nz_rtc.h"

#ifdef	__cplusplus
extern "C" {
#endif

BYTE mm_getunixtime(time_t* unix_time);
BYTE mm_gettime(struct tm * timedate);

#ifdef	__cplusplus
}
#endif

#endif	/* MM_WALLCLOCK_H */

