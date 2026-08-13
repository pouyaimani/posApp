// #include "stateMgr.h"
// #include "core.h"

// #define STATE_NUMBER_MAX 50

// static StateMgr __stateMgr;

// static ErrorDsc_t registerState(State* state, StateId1_t id) {
//     RETURN_VALUE_IF_NULL(state, ;, ERR_DSC_BAD_PARAMETER);

//     if (__stateMgr.count >= STATE_ID_MAX)
//         return ERR_DSC_NO_SPACE;

//     /* Prevent duplicate registration */
//     for (int i = 0; i < __stateMgr.count; ++i) {
//         if (__stateMgr.states[i].id == id)
//             return ERR_DSC_ALREADY_EXISTS;
//     }

//     __stateMgr.states[__stateMgr.count].id    = id;
//     __stateMgr.states[__stateMgr.count].state = state;
//     __stateMgr.count++;

//     return ERR_DSC_OK;
// }

// static State* get(StateId1_t id) {
//     size_t i;

//     for (i = 0; i < __stateMgr.count; ++i) {
//         if (__stateMgr.states[i].id == id)
//             return __stateMgr.states[i].state;
//     }

//     return NULL;
// }

// static ErrorDsc_t go2(StateId1_t id) {
//     State* next;

//     next = get(id);

//     if (!next)
//         return ERR_DSC_NOT_FOUND;

//     if (next == __stateMgr.current)
//         return ERR_DSC_OK;

//     __stateMgr.current = next;

//     smCore()->goTo(next);

//     return ERR_DSC_OK;
// }

// static ErrorDsc_t start(State* state, StateId1_t id) {
//     RETURN_VALUE_IF_NULL(state, ;, ERR_DSC_BAD_PARAMETER);
//     registerState(state, id);
//     go2(id);
//     smCore()->init(state);
//     smCore()->exec();
// }

// OOP_CTOR(StateMgr) {
//     self->start = start;
//     self->go2   = go2;
//     self->get   = get;
//     self->reg   = registerState;
// }

// StateMgr* stateMgr() {
//     CALL_ONCE(OOP_CALL_CTOR(StateMgr, &__stateMgr););
//     return &__stateMgr;
// }