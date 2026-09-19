#ifndef UPGRADE_H_
#define UPGRADE_H_

#include "oop.h"
#include <stdbool.h>
#include "state.h"

#define TMS_VER_ARR_SIZE 32

OOP_CLASS(Upgrade) { OOP_EXTENDS(State); };

OOP_CTOR(Upgrade, State* parent, const char* name);

#endif