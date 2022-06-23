/*
 ============================================================================
 Name        : mm_wallclock.c
 Author      : 
 Version     :
 Copyright   :
 Description : C, Ansi-style
 ============================================================================
 */

//#define THIS_IS_MAIN_FILE   //Uniquely identifies this as the file with the main application entry function main()

// Includes /////////////////////////////////////
#include <time.h>

#include "HardwareProfile.h"    //Required for all Netcruzer projects

//Add debugging. DEBUG_CONF_MAIN macro sets debugging to desired level (defined in projdefs.h)
#if !defined(DEBUG_CONF_MAIN)
    #define DEBUG_CONF_MAIN     DEBUG_CONF_DEFAULT   //Default Debug Level, disabled if DEBUG_LEVEL_ALLOFF defined, else DEBUG_LEVEL_ERROR
#endif
#define MY_DEBUG_LEVEL   DEBUG_CONF_MAIN
#include "nz_debug.h"
#include "nz_rtc.h"


/**
 * Get current time, and write values to given td (RTC_TIME_AND_DATE) structure.
 * This function blocks until data is read from RTC, which takes about 350uS when
 * the I2C bus is configured for 400kbps.
 *
 * The given RTC_TIME_AND_DATE structure has the following format:
 * td.hour The current time in Hours ( 0 to 23 )
 * td.min The current time in Minutes ( 0 to 59 )
 * td.sec The current time in seconds ( 0 to 59 )
 * td.day The current date, in day of the month ( 1 to 31 )
 * td.month The current date, in month of the year ( 1 to 12 )
 * td.year The current date, in years since 2000. For example, is 14 for 2014
 * td.wday The current date, in day of week ( 0 to 6 where Sunday = 0)
 *
 * For example:
 * @code
 * RTC_TIME_AND_DATE td;
 * rtcGetTimeAndDate(&td);
 * @endcode
 *
 * @param td Pointer to a RTC_TIME_AND_DATE structure containing new time and date
 * @param bcdFormat If 0, returned data is in standard binary format. If 1, it is in BCD format.
 * @return 0 if success, else error
 */
BYTE mm_getunixtime(time_t* unix_time) {
    BYTE ret;
    RTC_TIME_AND_DATE td;
    struct tm tt;    
      /*Get time and date in standard binary format*/
    ret = rtcGetTimeAndDate(&td, 0);
      /*copy*/
    tt.tm_sec = td.sec;
    tt.tm_min = td.min;
    tt.tm_hour = td.hour;
    tt.tm_mday = td.day;
      /*convert*/
    /*The current date, in month of the year ( 1 to 12 ) to
     * month ( 0 to 11 where January = 0 )*/  
    tt.tm_mon = td.month - 1;
    /*The current date, in years since 2000. For example, is 14 for 2014
     * years since 1900 */
    tt.tm_year = td.year + 100;
      /*copy*/
    tt.tm_wday = td.wday;
 /* tt.tm_yday = -1; */     /* day of year ( 0 to 365 where January 1 = 0 )*/
    tt.tm_isdst = -1;       /* Daylight Savings Time flag */
    *unix_time = mktime(&tt);
    return ret;
}

/**
 * Get current time, and write values to given td (RTC_TIME_AND_DATE) structure.
 * This function blocks until data is read from RTC, which takes about 350uS when
 * the I2C bus is configured for 400kbps.
 *
 * The given RTC_TIME_AND_DATE structure has the following format:
 * td.hour The current time in Hours ( 0 to 23 )
 * td.min The current time in Minutes ( 0 to 59 )
 * td.sec The current time in seconds ( 0 to 59 )
 * td.day The current date, in day of the month ( 1 to 31 )
 * td.month The current date, in month of the year ( 1 to 12 )
 * td.year The current date, in years since 2000. For example, is 14 for 2014
 * td.wday The current date, in day of week ( 0 to 6 where Sunday = 0)
 *
 * For example:
 * @code
 * RTC_TIME_AND_DATE td;
 * rtcGetTimeAndDate(&td);
 * @endcode
 *
 * @param td Pointer to a RTC_TIME_AND_DATE structure containing new time and date
 * @param bcdFormat If 0, returned data is in standard binary format. If 1, it is in BCD format.
 * @return 0 if success, else error
 */
BYTE mm_gettime(struct tm * timedate) { /* converts struct tm to ascii time */
    BYTE ret;
    RTC_TIME_AND_DATE td;
    struct tm tt;    
      /*Get time and date in standard binary format*/
    ret = rtcGetTimeAndDate(&td, 0);
      /*copy*/
    tt.tm_sec = td.sec;
    tt.tm_min = td.min;
    tt.tm_hour = td.hour;
    tt.tm_mday = td.day;
      /*convert*/
    /*The current date, in month of the year ( 1 to 12 ) to
     * month ( 0 to 11 where January = 0 )*/  
    tt.tm_mon = td.month - 1;
    /*The current date, in years since 2000. For example, is 14 for 2014
     * years since 1900 */
    tt.tm_year = td.year + 100;
      /*copy*/
    tt.tm_wday = td.wday;
 /* tt.tm_yday = -1; */     /* day of year ( 0 to 365 where January 1 = 0 )*/
    tt.tm_isdst = -1;       /* Daylight Savings Time flag */
    memcpy(timedate, &tt, sizeof(struct tm));
    return ret;
}
