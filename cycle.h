/*
 * cycle.h
 *
 *  Created on: May 31, 2020
 *      Author: mmtools
 */

#ifndef CYCLE_H_
#define CYCLE_H_

int cycleInit();
void cycleEnQueue(DWORD_VAL elapsed_msec32, WORD elapsed_msec, BOOL transition);
int cycleTask();
INT16 initDataTable(BYTE** pByteArray, WORD* pSize);
INT16 makeDataTable(BYTE** pByteArray, WORD* pSize);

#define CYCLE_NUM_REC    10

typedef struct __PACKED
{
    INT32 dt;
    INT32 count;
    struct __PACKED
    {
        INT16 integer;
        INT16 decimal;
    } seconds;
    struct __PACKED
    {
        INT16 integer;
        INT16 decimal;
    } average;    
} TABLE_RECORD;

typedef struct  __PACKED
{
    char name[50] __PACKED;
    INT32 date_time;
    INT16 record_count;
    TABLE_RECORD record[CYCLE_NUM_REC] __PACKED;
} TABLE;

#endif /* CYCLE_H_ */
