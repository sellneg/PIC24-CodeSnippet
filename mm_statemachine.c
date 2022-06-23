// System includes
#include <stdio.h>

// Local includes
#include "mm_statemachine.h"

typedef struct {
    const char * name;
    void (*func)(void);
} stateFunctionRow_t;

/// \brief  Maps a state to it's state transition function, which should be called
///         when the state transitions into this state.
/// \warning    This has to stay in sync with the state_t enum!
static stateFunctionRow_t stateFunctionA[] = {
      // NAME               // FUNC
    { "ST_IDLE",            &cycle_idle },
    { "ST_RUNNING",         &cycle_running },
    { "ST_MOLD_CLOSED",     &cycle_mold_closed },
    { "ST_INCOMPLETE",      &cycle_incomplete },
    { "ST_MOLD_OPEN",       &cycle_mold_open },
    { "ST_COMPLETE",        &cycle_complete },
    { "ST_INPUT_ERROR",     &cycle_input_error },
};

typedef struct {
    state_t currState;
    event_t event;
    state_t nextState;
} stateTransMatrixRow_t;

static stateTransMatrixRow_t stateTransMatrix[] = {
    // CURR STATE       // EVENT            // NEXT STATE
    { ST_IDLE,          EV_RISING_EDGE,    ST_MOLD_CLOSED  },
    { ST_IDLE,          EV_FALLING_EDGE,     ST_INPUT_ERROR  },
    { ST_IDLE,          EV_TIME_OUT,        ST_IDLE  },
    { ST_MOLD_CLOSED,   EV_RISING_EDGE,    ST_INPUT_ERROR  },
    { ST_MOLD_CLOSED,   EV_FALLING_EDGE,     ST_MOLD_OPEN  },
    { ST_MOLD_CLOSED,   EV_TIME_OUT,        ST_INCOMPLETE  },
    { ST_MOLD_OPEN,     EV_RISING_EDGE,    ST_COMPLETE  },
    { ST_MOLD_OPEN,     EV_FALLING_EDGE,     ST_INPUT_ERROR  },
    { ST_MOLD_OPEN,     EV_TIME_OUT,        ST_INCOMPLETE  },
    { ST_INCOMPLETE,    EV_RISING_EDGE,    ST_MOLD_CLOSED  },
    { ST_INCOMPLETE,    EV_FALLING_EDGE,     ST_INPUT_ERROR  },
    { ST_INCOMPLETE,    EV_TIME_OUT,        ST_IDLE  },
    { ST_COMPLETE,      EV_DEFAULT,         ST_MOLD_CLOSED  },
    { ST_INPUT_ERROR,   EV_DEFAULT,         ST_RUNNING  },
    { ST_RUNNING,       EV_RISING_EDGE,    ST_MOLD_CLOSED  },
    { ST_RUNNING,       EV_FALLING_EDGE,     ST_INPUT_ERROR  },
    { ST_RUNNING,       EV_TIME_OUT,        ST_IDLE  },
};

void mmStateMachine_Init(stateMachine_t * stateMachine) {
    printf("Initializing state machine.\r\n");
    stateMachine->currState = ST_IDLE;
}

void mmStateMachine_RunIteration(stateMachine_t *stateMachine, event_t event) {

    // Iterate through the state transition matrix, checking if there is both a match with the current state
    // and the event
    unsigned long i;
    for(i = 0; i < sizeof(stateTransMatrix)/sizeof(stateTransMatrix[0]); i++) {
        if(stateTransMatrix[i].currState == stateMachine->currState) {
            if((stateTransMatrix[i].event == event) || (stateTransMatrix[i].event == EV_ANY)) {

                // Transition to the next state
                stateMachine->currState =  stateTransMatrix[i].nextState;

                // Call the function associated with transition
                (stateFunctionA[stateMachine->currState].func)();
                break;
            }
        }
    }
}

const char * mmStateMachine_GetStateName(state_t state) {
    return stateFunctionA[state].name;
}