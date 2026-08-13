// #ifndef STATE_MGR_H_
// #define STATE_MGR_H_

// #include "state.h"
// #include "error.h"

// #define STATE_ID_MAX 50

// typedef int StateId1_t;

// typedef struct {
//     StateId1_t id;
//     State*     state;
// } States;

// OOP_CLASS(StateMgr) {
//     OOP_METHOD(ErrorDsc_t, reg, State * state, StateId1_t id);
//     OOP_METHOD(ErrorDsc_t, start, State * state, StateId1_t id);
//     OOP_METHOD(ErrorDsc_t, go2, StateId1_t id);
//     OOP_METHOD(State*, get, StateId1_t id);
//     States   states[STATE_ID_MAX];
//     State*   current;
//     uint16_t count;
// };

// StateMgr* stateMgr();

// #endif