#include "state.h"
#include <stdbool.h>

typedef enum StateId_t {
    STATE_ID_START_UP,
    STATE_ID_INFO,
    STATE_ID_GINFO,
    STATE_ID_IDLE,
    STATE_ID_INPUT,
    STATE_ID_CARD_HOLDER,
    STATE_ID_DIALOGUE,
    STATE_ID_SUPPORTER,
    STATE_ID_DEV_INFO,
    STATE_ID_MENU
} StateId_t;

State *getState(StateId_t id);


/*********************Startup*********************/

OOP_CLASS(Startup) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Startup, State *parent, const char *name);

/**********************Idle**********************/

OOP_CLASS(Idle) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Idle, State *parent, const char *name);

/**********************Input**********************/

typedef enum {
    IN_MODE_AMOUNT,
    IN_MODE_PASSWORD,
    IN_MODE_NUMBERS,
    IN_MODE_ALPHAB
} InputMode_t;

OOP_CLASS(InputCfg) {
    const char *title;
    uint8_t maxLen;
    InputMode_t mode;
    State *next;
    State *prev;
};

OOP_CLASS(Input) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setMode, InputMode_t);
    OOP_METHOD(void, setData, const char *, const char *);
    OOP_METHOD(void, setAmount, const char *);
    OOP_METHOD(void, setMax, int);
    OOP_METHOD(void, reset);
    char *input;
    char *password;
};

OOP_CTOR(Input, State *parent, const char *name);

#define GOTO_INPUT(prev, next, title, body, max, mode)          \
    Input *in = (Input*)getState(STATE_ID_INPUT);               \
    in->reset();                                                \
    in->setData(title, body);                                   \
    in->setMax(max);                                            \
    in->setMode(mode);                                          \
    OOP_CALL(getState(STATE_ID_INPUT), setPrev, prev);           \
    OOP_CALL(getState(STATE_ID_INPUT), setNext, next);           \
    SM_GOTO(getState(STATE_ID_INPUT));  

/**********************Card Holder**********************/

OOP_CLASS(CardHolder) {
    OOP_EXTENDS(State);
    bool isMagSwiped;
};

OOP_CTOR(CardHolder, State *parent, const char *name);

/**************************Info*************************/

OOP_CLASS(Info) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setText, const char *, const char *);
};

OOP_CTOR(Info, State *parent, const char *name);

#define GOTO_INFO(prev, next, title, body)                      \
    Info *info = (Info *)getState(STATE_ID_INFO);               \
    info->setText(title, body);                                 \
    OOP_CALL(getState(STATE_ID_INFO), setPrev, prev);           \
    OOP_CALL(getState(STATE_ID_INFO), setNext, next);           \
    SM_GOTO(getState(STATE_ID_INFO));                           

/************************Dev Info*************************/

OOP_CLASS(DevInfo) {
    OOP_EXTENDS(State);
};

OOP_CTOR(DevInfo, State *parent, const char *name);

#define GOTO_DEV_INFO(prev)                                     \
    DevInfo *devInfo = (DevInfo *)getState(STATE_ID_DEV_INFO);  \
    OOP_CALL(getState(STATE_ID_DEV_INFO), setPrev, prev);       \
    OOP_CALL(getState(STATE_ID_DEV_INFO), setNext, prev);       \
    SM_GOTO(getState(STATE_ID_DEV_INFO));                    

/**************************Ginfo*************************/

OOP_CLASS(Ginfo) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setText, const char *);
    OOP_METHOD(void, setImg, const char *);
};

OOP_CTOR(Ginfo, State *parent, const char *name);

#define GOTO_GINFO(state, text, img)                            \
    OOP_CALL(getState(STATE_ID_GINFO), setNext, state);         \
    Ginfo *info = (Ginfo *)getState(STATE_ID_GINFO);            \
    info->setText(title);                                       \
    info->setImg(body);                                         \
    SM_GOTO(getState(STATE_ID_GINFO));   

/************************Dialogue***********************/

OOP_CLASS(Dialogue) {
    OOP_EXTENDS(State);
    OOP_METHOD(void, setBody, const char *);
    OOP_METHOD(void, setTitle, const char *);
};

OOP_CTOR(Dialogue, State *parent, const char *name);

#define GOTO_DIAL(prev, next, title, body)                      \
    OOP_CALL(getState(STATE_ID_DIALOGUE), setNext, next);       \
    OOP_CALL(getState(STATE_ID_DIALOGUE), setPrev, prev);       \
    Dialogue *dial = (Dialogue *)getState(STATE_ID_DIALOGUE);   \
    dial->setTitle(title);                                      \
    dial->setBody(body);                                        \
    SM_GOTO(getState(STATE_ID_DIALOGUE));        


/************************Supporter***********************/

OOP_CLASS(Supporter) {
    OOP_EXTENDS(State);
};

OOP_CTOR(Supporter, State *parent, const char *name);

/************************ Menu ***********************/
typedef struct Menu Menu;

OOP_CLASS(StMenu) {
    OOP_EXTENDS(State);
    Menu *menu;
};

OOP_CTOR(StMenu, State *parent, const char *name);

#define GOTO_MENU(prev, amenu)                                  \
    StMenu *stMenu = (StMenu *)getState(STATE_ID_MENU);         \
    stMenu->menu = amenu;                                       \
    OOP_CALL(getState(STATE_ID_MENU), setPrev, prev);           \
    SM_GOTO(getState(STATE_ID_MENU));