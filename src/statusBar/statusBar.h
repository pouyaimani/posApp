#ifndef STATUSBAR_H_
#define STATUSBAR_H_

#include "oop.h"

typedef enum {
    STBAR_INFO_DATE_TIME,
    STBAR_INFO
} StatusBarInfoMode_t;

OOP_CLASS(StatusBar) {
    int dummy;
    OOP_METHOD(void, enDateTimeMode);
    OOP_METHOD(void, setInfo, const char *);
    OOP_METHOD(void, setSoundVolume, int);
};

OOP_CTOR(StatusBar);

StatusBar *statusBar();

#endif