#ifndef CARDHOLDER_H_
#define CARDHOLDER_H_

#include "txn.h"

typedef struct {
    const char* name;
    bool        enabled;
    uint64_t    usage;
    void (*handler)();
} CardHolderItems;

CardHolderItems* cardHolderItems();

extern size_t LEN_CARDHOLDER_ITEMS;

#endif