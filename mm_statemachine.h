#ifndef MM_STATE_MACHINE_H_
#define MM_STATE_MACHINE_H_

typedef enum {
    ST_IDLE,
    ST_RUNNING,
    ST_MOLD_CLOSED,
    ST_INCOMPLETE,
    ST_MOLD_OPEN,
    ST_COMPLETE,
    ST_INPUT_ERROR,
} state_t;

typedef struct {
    state_t currState;
} stateMachine_t;

typedef enum {
    EV_ANY,
    EV_NONE,
    EV_FALLING_EDGE,
    EV_RISING_EDGE,
    EV_TIME_OUT,
    EV_DEFAULT,
} event_t;

void mmStateMachine_Init(stateMachine_t * stateMachine);
void mmStateMachine_RunIteration(stateMachine_t *stateMachine, event_t event);
const char * mmStateMachine_GetStateName(state_t state);

extern void cycle_idle(void);
extern void cycle_running(void);
extern void cycle_mold_closed(void);
extern void cycle_incomplete(void);
extern void cycle_mold_open(void);
extern void cycle_complete(void);
extern void cycle_input_error(void);

#endif // #ifndef MM_STATE_MACHINE_H_