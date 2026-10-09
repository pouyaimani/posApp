#ifndef PICC_T3RTOS_H_
#define PICC_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../picc.h"

/*
 * Default EMV/contactless configuration.
 *
 * Add PICC_TECH_FELICA if the application requires it.
 */
#define PICC_T3_DEFAULT_TECH (PICC_TECH_A | PICC_TECH_B)

OOP_CLASS(PiccT3Rtos) { OOP_EXTENDS(Picc); };

OOP_CTOR(PiccT3Rtos);

#endif

#endif