#include "nth_queue.h"

void nth_queueInit(NtEventQueue* q) {
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
}

bool nth_queuePush(NtEventQueue* q, Event* ev) {
    if (!q || !ev)
        return false;

    if (q->count >= NT_MAX_EVENTS)
        return false;

    q->events[q->tail] = ev;

    q->tail++;
    q->tail %= NT_MAX_EVENTS;

    q->count++;

    return true;
}

Event* nth_queuePop(NtEventQueue* q) {
    Event* ev;

    if (!q)
        return NULL;

    if (q->count == 0)
        return NULL;

    ev = q->events[q->head];

    q->head++;
    q->head %= NT_MAX_EVENTS;

    q->count--;

    return ev;
}