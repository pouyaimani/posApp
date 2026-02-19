#include "states.h"
#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "utility/utility.h"
#include "font/myFont.h"
#include "myColor.h"
#include "ui/ui.h"

InfoPage page;

STATE_DEF_ENTER(Info) {
    OOP_CALL(&page, show);
}

STATE_DEF_EXIT(Info) {
    OOP_CALL(&page, hide);
}

STATE_DEF_HANDLE(Info, TimeOutEvent) {

}

STATE_DEF_HANDLE(Info, KeypadEvent) {
    SM_GOTO(state->next);
}

static void setText(const char *title, const char *body) {
    OOP_CALL(&page, setData, INFO_T_TEXT, title, body);
}

OOP_CTOR(Info, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Info);
    self->base.vtable.exit = STATE_EXIT(Info);
    self->base.vtable.handleKeypad = STATE_HANDLE(Info, KeypadEvent);
    self->base.vtable.handleTimeout = STATE_HANDLE(Info, TimeOutEvent);
    self->setText = setText;
    page = infoPage();
}