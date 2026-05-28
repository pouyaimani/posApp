#ifndef NT_QUEUE_H
#define NT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"
#include "nth_config.h"

typedef struct {

    Event *events[NT_MAX_EVENTS];

    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;

} NtEventQueue;

void nth_queueInit(NtEventQueue *q);

bool nth_queuePush(
    NtEventQueue *q,
    Event *ev);

Event *nth_queuePop(
    NtEventQueue *q);

#endif