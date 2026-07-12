#ifndef INPUT_CFG_H
#define INPUT_CFG_H

#include <stdint.h>
#include "phrases/phrases.h"

typedef enum {
    INPUT_TYPE_KEYPAD,
    INPUT_TYPE_PED,
} InputType;

typedef enum {
    INMD_ENTER_AMOUNT,
    INMD_ENTER_PIN,
    INMD_ENTER_NUMBERS,
    INMD_ENTER_ALPHAB,
    INMD_ENTER_TIME,
    INMD_ENTER_DATE,
    INMD_ENTER_IP,
    INMD_ALL
} InputMode;

typedef struct {
    InputType type;
    InputMode mode;
    char*     title;
    char*     info;
    uint16_t  maxLen;
    uint16_t  minLen;
} InputCfg;

#endif