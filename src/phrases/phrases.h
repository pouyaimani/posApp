#ifndef PHRASES_H_
#define PHRASES_H_

#include "oop.h"
#include "common.h"

typedef struct {
    const char* text[LNG_COUNT];
} PhraseEntry;

typedef enum {
#define X(id, textFa, textEn) id,
#include "phrases.def"
#undef X

    PHRASE_T_COUNT
} Phrases_t;

const char* phraseGet(Phrases_t id, Language_t);

// Get phrase with device default language
const char* phraseGetDef(Phrases_t id);

void getResponseCode(int code, char* message, size_t size);

#endif