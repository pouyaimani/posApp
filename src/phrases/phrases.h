#ifndef PHRASES_H_
#define PHRASES_H_

#include "oop.h"

typedef enum {
    LANG_EN,
    LANG_FA,
    LANG_COUNT
} Lang_t;

typedef struct {
    const char *text[LANG_COUNT];
} PhraseEntry;

typedef enum {
#define X(id, textFa, textEn) id,
#include "phrases.def"
#undef X

    PHRASE_COUNT
} Phrases_t;

const char *phraseGet(Phrases_t id, Lang_t);

#endif