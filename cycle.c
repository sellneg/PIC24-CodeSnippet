/*
 ============================================================================
 Name        : cycle.c
 Author      : 
 Version     :
 Copyright   : 
 Description :  C, Ansi-style
 ============================================================================
 */
//System include files
#include <time.h> // for clock 
#include <stdio.h> // for printf 

//Netcruzer include files
#include "HardwareProfile.h"

//XC16 Compiler and Microchip include files
#include <PPS.h>
#include "USB/usb.h"            //Includes headers for Microchip USB
#include "TCPIP Stack/TCPIP.h"  //Includes headers for Microchip TCPIP Stack

//Remaining defines
#include "mainApp.h"
#include "nz_netcruzer.h"
#include "nz_analog.h"
//#include "nz_pt66ei24p.h"
#include "nz_interrupt.h"
#include "nz_rtc.h"
#include "cycle/cycle.h"
#include "mm_statemachine.h"
#include "mm_wallclock.h"

//#define G_DEBUG
//Add debugging. DEBUG_CONF_MAIN macro sets debugging to desired level (defined in projdefs.h)
#if !defined(DEBUG_CONF_MAIN)
    #define DEBUG_CONF_MAIN        DEBUG_CONF_DEFAULT   //Default Debug Level, disabled if DEBUG_LEVEL_ALLOFF defined, else DEBUG_LEVEL_ERROR
#endif
#define MY_DEBUG_LEVEL   DEBUG_CONF_MAIN
#include "nz_debug.h"

#define NANO        1000000000L
#define MICRO       1000000L
#define MILLI       1000L

/*
 * Injection Molding Machine Status
 */
 #define IMM_OFF        0
 #define IMM_IDLE       1
 #define IMM_RUN        2
 #define IMM_DRYCYCLE   3
 #define SM_STARTUP     4

/*
 * Cycle Type
 */
 #define CYCLETYPE_NORMAL  0
 #define CYCLETYPE_SHORT   3
 #define CYCLETYPE_LONG    1

/*
 * Old
 */
 #define IDLE_TIMEOUT 300     // 2 minutes in mseconds

// Create new state machine object
stateMachine_t stateMachine;

/*
 *  flags   bit 0   edge    
 *              1   timeout
 *              2   tick_overflow
 *          3 - 7   not used
 */
typedef struct __PACKED {
    DWORD   time32Bitmsec;
    WORD    time16Bitmsec;
    UINT16_BITS flags;
} CYCLE_PKT;

#define CYCLE_PKT_SIZE sizeof(CYCLE_PKT)

typedef struct {
   DWORD stamp;
   DWORD wallclock;   
} plunger_data;

typedef struct {
   plunger_data in, out, last;
   DWORD count;
   int mode, type;
   int msqid;
   DWORD elapsed;   // msec
   CYCLE_PKT event;
} cycle_data;

cycle_data * c;

/******************************************************************************
 *             INIT
 ******************************************************************************/

CIRBUF cbufFifo;
BYTE fifoBuf[64];   // size must be one of 8,16,32,64,128,512,1024,2048,4096,8192,16384....
CIRBUF cbufCyclesTable;
BYTE bufCyclesTable[512];   // size must be one of 8,16,32,64,128,512,1024,2048,4096,8192,16384....

/* Prototypes */
void cyclefree();
int cyclerun();
int cyclestop();
int cycleProcess(CYCLE_PKT *pEvent);
void print_cycledata(void);
void tableEnQueue(DWORD timestamp, DWORD count, DWORD time, WORD mold_temp);

WORD tmrCycleTimeout = 0;   //Timer machine timeout (16-bit, 1ms Timer)
/************************** cycleInit *****************************************/
int cycleInit(void) {
    c = malloc(sizeof(cycle_data));
    if(!c) printf("\nCycle data NOT allocated ");
    /* Init Cycle Data */
    c->count = 0; // Read back from somewhere?
    c->mode = SM_STARTUP;
    c->type = CYCLETYPE_NORMAL;    
    //Create and initialize a "Circular Buffer". It is initialized as a "Packet Buffer". This
    //means we can write/read packets to/from the "Circular Buffer". It is VERY IMPORTANT not
    //to use normal put and get functions for adding/removing data to a packet buffer! It will
    //corrupt the buffer!

    cbufInit(&cbufFifo, fifoBuf, sizeof(fifoBuf), CIRBUF_FORMAT_NONE | CIRBUF_TYPE_PACKET);
    //Do some checks on buffer
    //<b>Output:</b> "Buffer does not have any packets"

    CIRBUF * pcbufCyclesTable = &cbufCyclesTable;
    cbufInit(pcbufCyclesTable, bufCyclesTable, sizeof(bufCyclesTable), CIRBUF_FORMAT_NONE | CIRBUF_TYPE_PACKET);

    //    threshold = db_get_cycletimeout(IDLE_TIMEOUT);
    /* This global variable can be changed in function handling signal */
    //running = 1;
    /* create state machine */
    mmStateMachine_Init(&stateMachine);
#ifdef G_DEBUG
    printf("State is now %s.", mmStateMachine_GetStateName(stateMachine.currState));
#endif

    tmrCycleTimeout = tick16Get() + tick16ConvertFromMS(30000); //Timeout delay
    
    return EXIT_SUCCESS;
}

/******************************************************************************
 *             EnQueue to FIFO
 ******************************************************************************/
void cycleEnQueue(DWORD_VAL elapsed_msec32, WORD elapsed_msec, BOOL transition) {
    CYCLE_PKT packet;
    WORD ret;
    DWORD val = (tick_val.Val - elapsed_msec32.Val);

    if (val > 60000) debugPutString("\nToo long");      // Don't ever write to tick_val !!!!
#ifdef G_DEBUG
    debugPutString("\nTime Elapsed in ms = ");
    debugPutWord(tick16GetElapsedMS((TICK16)elapsed_msec));
    if (!transition) debugPutString(" CLOSED");         // Previous State
    else debugPutString(" OPEN");
#endif
    if (cbufGetType(&cbufFifo) != CIRBUF_TYPE_PACKET)
        printf("\nNOT PACKET BUFFER!!!");
    else {
        packet.flags.Val = 0;
        packet.flags.bits.b0 = transition;   // Edge 1 = rising, 0 = falling
        packet.time16Bitmsec = tick16GetElapsedMS((TICK16)elapsed_msec);
        packet.time32Bitmsec = val;
        ret = cbufPutPacket(&cbufFifo, (BYTE *)&packet, CYCLE_PKT_SIZE);     //Add this BYTE array to our "Circular Buffer" as a packet.
    }
}

/******************************************************************************
 *             PROCESS CYCLES TASK
 ******************************************************************************
 * 
 *  Entrypoint into periodic cycle processing
 * 
 *  Called from Application's main loop
 * 
 ******************************************************************************/
int cycleTask(void) {
    BYTE* pByte;
    WORD size;
    CYCLE_PKT Event;
    //Do some checks on buffer
    if (cbufHasWholePacket(&cbufFifo)) {
        /* 
         *  TODO:
         *     State Machine
         */
        size = cbufGetContiguousPacket(&cbufFifo, &pByte);      //Get pointer to packet data. Nothing is removed!
        memcpy(&Event, pByte, size);        //Below we will remove and print string
        cbufRemovePacket(&cbufFifo);        //Remove packet
        
        cycleProcess(&Event);

        // Check timeout timer
        tmrCycleTimeout = tick16Get() + tick16ConvertFromMS(30000); //Timeout delay
#ifdef NO_ONE_SHALL_PASS

                /*****   TIMEOUT!!!!   ****/
                mmStateMachine_RunIteration(&stateMachine, EV_TIME_OUT);
                printf("State is now %s.\r\n", mmStateMachine_GetStateName(stateMachine.currState));

#endif
	}
    else {
        
        // Check timeout timer
        if ((tmrCycleTimeout!=-1) && tick16TestTmr(tmrCycleTimeout)) {
            tmrCycleTimeout = -1;       //Don't trigger this code again! Only a one time start-up event!
#ifdef G_DEBUG
            printf("\nTimeOut!!!!");
#endif
            /*****   TIMEOUT!!!!   ****/
            mmStateMachine_RunIteration(&stateMachine, EV_TIME_OUT);
#ifdef G_DEBUG
            printf("State is now %s.\r\n", mmStateMachine_GetStateName(stateMachine.currState));
#endif
        }
    }

    return EXIT_SUCCESS;
}

/******************************************************************************
*             PROCESS CYCLES TASK
******************************************************************************/
int cycleProcess(CYCLE_PKT *pEvent) {

    int no_activity = 0;
    
#ifdef G_DEBUG
    printf("\nrecvd: %lu, %u, %04X",
                pEvent->time32Bitmsec,
                pEvent->time16Bitmsec,
                pEvent->flags.Val);
#endif
    
    c->event.flags = pEvent->flags;
    c->event.time16Bitmsec = pEvent->time16Bitmsec;
    c->event.time32Bitmsec = pEvent->time32Bitmsec;
    
    switch (pEvent->flags.bits.b0) {
        case 0:
#ifdef G_DEBUG
            printf("Falling Edge.\r\n");
#endif
/       
            mmStateMachine_RunIteration(&stateMachine, EV_FALLING_EDGE);
#ifdef G_DEBUG
            printf("State is now %s.\r\n", mmStateMachine_GetStateName(stateMachine.currState));
#endif
            break;           
        case 1:
#ifdef G_DEBUG
            printf("Rising Edge.\r\n");
#endif
       
            mmStateMachine_RunIteration(&stateMachine, EV_RISING_EDGE);
#ifdef G_DEBUG
            printf("State is now %s.\r\n", mmStateMachine_GetStateName(stateMachine.currState));
#endif
            break;
    } 

    return no_activity;
}

/******************************************************************************
*             EXPORT TO STATE MACHINE
******************************************************************************/
void cycle_idle(void) {
//   DWORD wallclock, stamp;

#ifdef G_DEBUG
   printf("cycle_idle() called.\r\n");
#endif
   c->mode = IMM_IDLE;
   c->event.flags.Val = 0;
   c->event.time16Bitmsec = 0;
   c->event.time32Bitmsec = 0;
   c->elapsed = 0;

   mmStateMachine_RunIteration(&stateMachine, EV_DEFAULT);
}
void cycle_running(void) {
   c->mode = IMM_RUN;
//   db_update_runstatus(c->mode);
#ifdef G_DEBUG
   printf("cycle_running() called.\r\n");
#endif
}
void cycle_mold_closed() {
    c->mode = IMM_RUN;
    c->event.flags.Val = 0;
    c->event.time16Bitmsec = 0;
    c->event.time32Bitmsec = 0;

#ifdef G_DEBUG
    printf("cycle_mold_closed() called.\r\n");
#endif
}
void cycle_incomplete() {
   /* TODO make this update a cycle record if not full-cycle and mark as 'short' */

    if ((c->event.time16Bitmsec != 0) && (c->event.time32Bitmsec != 0))
        c->count = c->count + 1;
    c->event.flags.Val = 0;
    c->event.time16Bitmsec = 0;
    c->event.time32Bitmsec = 0;
#ifdef G_DEBUG
    printf("cycle_incomplete() called.\r\n");
#endif
    print_cycledata();
    c->elapsed = 0;    
}
void cycle_mold_open() {
   c->mode = IMM_RUN;
    if (c->event.time32Bitmsec) {
        if (c->event.time32Bitmsec > c->event.time16Bitmsec)
            c->elapsed = c->event.time32Bitmsec;
        else
            c->elapsed = c->event.time16Bitmsec;
    }
    c->event.flags.Val = 0;
    c->event.time16Bitmsec = 0;
    c->event.time32Bitmsec = 0;
//   db_update_runstatus(c->mode);
#ifdef G_DEBUG
    printf("cycle_mold_open() called.\r\n");
#endif
}
void cycle_complete() {
    time_t timestamp;
    WORD mold_temp;
    
    c->mode = IMM_RUN;
    c->count = c->count + 1;
    if (c->event.time32Bitmsec) {
        if (c->event.time32Bitmsec > c->event.time16Bitmsec)
            c->elapsed = c->elapsed + c->event.time32Bitmsec;
        else
            c->elapsed = c->elapsed + c->event.time16Bitmsec;
    }
    c->event.flags.Val = 0;
    c->event.time16Bitmsec = 0;
    c->event.time32Bitmsec = 0;

#ifdef G_DEBUG
    printf("cycle_complete() called. Cycle time: %ld mseconds\r\n", c->elapsed);
#endif

    mm_getunixtime(&timestamp);
    mold_temp = 0;
    tableEnQueue(timestamp, c->count, c->elapsed, mold_temp);
    print_cycledata();
    c->elapsed = 0;
    mmStateMachine_RunIteration(&stateMachine, EV_DEFAULT);
}
void cycle_input_error() {

#ifdef G_DEBUG
   printf("cycle_input_error() called.\r\n");
#endif

   mmStateMachine_RunIteration(&stateMachine, EV_DEFAULT);
}

/******************************************************************************
*             Housekeeping
******************************************************************************/
void print_cycledata(void) {
    DEBUG_PUT_STR(DEBUG_LEVEL_INFO, "\nCount\tCycle Time\n");
    DEBUG_PUT_WORD(DEBUG_LEVEL_INFO, c->count);
    DEBUG_PUT_STR(DEBUG_LEVEL_INFO, "\t");
    DEBUG_PUT_WORD(DEBUG_LEVEL_INFO, c->elapsed);
    DEBUG_PUT_STR(DEBUG_LEVEL_INFO, " ms\n");
}

/**
 * Structure containing information on board status
 */
typedef struct __PACKED {
   DWORD timestamp; /* seconds (unix time)  */
   DWORD count;     /* cycle count          */
   DWORD time;      /* cycle time msec      */
   WORD mold_temp;  /* mold temperature     */
} CYCLE_REC;
#define CYCLE_REC_SIZE sizeof(CYCLE_REC)
DWORD averageCycleTime = 0;                 /*Keep moving average*/
/******************************************************************************
*             Manage Cycle Table
******************************************************************************/
void tableEnQueue(DWORD timestamp, DWORD count, DWORD time, WORD mold_temp) {
    CYCLE_REC record;
    WORD ret;
    
#ifdef G_DEBUG
    debugPutString("\nTime Elapsed in ms = ");
    debugPutWord((WORD)time);
#endif
    if (cbufGetType(&cbufCyclesTable) != CIRBUF_TYPE_PACKET)
        printf("\nNOT PACKET BUFFER!!!");
    else {
        record.timestamp = timestamp;
        record.count = count;
        record.time = time;
        record.mold_temp = mold_temp;
        /*Add this BYTE array to our "Circular Buffer" as a packet*/
        if (cbufGetFreeForPacket(&cbufCyclesTable) < CYCLE_REC_SIZE)
            cbufRemovePacket(&cbufCyclesTable);        //Remove packet
        ret = cbufPutPacket(&cbufCyclesTable, (BYTE *)&record, CYCLE_REC_SIZE);
#ifdef G_DEBUG
        printf("\n %d bytes added", ret);
        printf("\nFifo Packet count is now %d", cbufGetCount(&cbufCyclesTable)/CYCLE_REC_SIZE);
#endif
    }
}
/******************************************************************************
*             Manage Cycle Table
******************************************************************************/
static TABLE dtabUserData;

INT16 initDataTable(BYTE** pByteArray, WORD* pSize) {
    WORD ret;
    TABLE *p = &dtabUserData;
    
    *pSize = sizeof(TABLE);
    *pByteArray = (BYTE*)&dtabUserData;
    
    memset(p, 0, sizeof(JACK));
    strcpy(p->name, "mold1234");
    time_t unix_time;
    ret = mm_getunixtime(&unix_time);
    p->date_time = unix_time;
    p->record_count = 0;
    
#ifdef G_DEBUG
    char strBuffer[50];
    sprintf(strBuffer, "(%s)Epoch = %ld\n", __FUNCTION__, p->date_time);
    DEBUG_PUT_STR(DEBUG_LEVEL_INFO, strBuffer);
    printf("\nSize of Table %d bytes\n", sizeof(JACK));
    printf("Table has %d records\n", 0);
#endif    
    return ret;
}

INT16 makeDataTable(BYTE** pByteArray, WORD* pSize) {
    WORD ret;
    BYTE* pByte;
    WORD size;
    CYCLE_REC r;
    UINT8 index = 0;
    TABLE *p = &dtabUserData;
    WORD  sizeBlock;
    
    *pSize = sizeof(JACK);
    *pByteArray = (BYTE*)&dtabUserData;

    while ((cbufGetCount(&cbufCyclesTable)) && (index < CYCLE_NUM_REC)) {
        /*Get pointer to packet data. Nothing is removed!*/
        size = cbufGetContiguousPacket(&cbufCyclesTable, &pByte);
        memcpy(&r, pByte, size);        //Below we will remove and print string
        cbufRemovePacket(&cbufCyclesTable);  //Remove packet
        if ((p->record_count == 0) || (averageCycleTime == 0)) {
            averageCycleTime = r.time;
        } else {
            averageCycleTime = ((averageCycleTime * (r.count - 1) + r.time) / r.count);
        }
        if (p->record_count >= 1) {
           if (p->record_count < CYCLE_NUM_REC) {
                sizeBlock = p->record_count * sizeof(JACK_SHIT);
           } else {
                sizeBlock = CYCLE_NUM_REC * sizeof(JACK_SHIT);
           }
#ifdef G_DEBUG
           printf("\nMove Block size %d\n", sizeBlock);
#endif
           memmove(&p->record[1], &p->record[0], sizeBlock);
        }
        p->record[0].dt = r.timestamp;
        p->record[0].count = r.count;
        p->record[0].seconds.integer = r.time/1000;
        p->record[0].seconds.decimal = (r.time%1000)/100;
        p->record[0].average.integer = averageCycleTime/1000;
        p->record[0].average.decimal = (averageCycleTime%1000)/100;
        
        p->record_count++;
        if (p->record_count >= CYCLE_NUM_REC)
            p->record_count = CYCLE_NUM_REC;
        index++;
    }
    if (index > 0) {
        strcpy(p->name, "mold1234");
        time_t unix_time;
        ret = mm_getunixtime(&unix_time);
        p->date_time = unix_time;
    }
#ifdef G_DEBUG
    char strBuffer[50];
    sprintf(strBuffer, "(%s)Epoch = %ld\n", __FUNCTION__, p->date_time);
    DEBUG_PUT_STR(DEBUG_LEVEL_INFO, strBuffer);
    printf("\nSize of Table %d bytes\n", sizeof(JACK));
    printf("Table has %d records\n", index);
#endif
    
    return index;
}