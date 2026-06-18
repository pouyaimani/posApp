#ifndef TMS_H_
#define TMS_H_

#include "oop.h"
#include <stdbool.h>
#include "state.h"

#define TMS_VER_ARR_SIZE 32

OOP_CLASS(TmsVersion) { char data[TMS_VER_ARR_SIZE]; };

OOP_CLASS(TmsData) { char curVersion; };

OOP_CLASS(TMS) { OOP_EXTENDS(State); };

OOP_CTOR(TMS, State* parent, const char* name);

#endif