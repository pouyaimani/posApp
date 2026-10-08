#include "eventAll.h"
#include "core.h"
#include "logger.h"
#include "error.h"

#include <stddef.h>

#define EVENT_REGISTRY_CAPACITY 32

typedef struct {
    EventId        id;
    EventFactoryFn factory;
} EventEntry;

static EventEntry registry[EVENT_REGISTRY_CAPACITY];
static uint16_t   registryCount = 0;

/* ================= Base Event ================= */

static void dispatch(Event* self, State* state) {
    if (!self)
        return;

    self->target = state;
    smCore()->raiseEvent(self);
}

/* ================= Registry ================= */

bool eventRegister(EventId id, EventFactoryFn factory) {
    LOG_TRACE("Registering event with [id, factory fn] = [%s%d, %p%s]",
              LOG_COLOR_RED, id, factory, LOG_COLOR_RESET);
    RETURN_VALUE_IF_NULL(factory, ;, false);

    /* Reject duplicate event IDs. */
    for (uint16_t i = 0; i < registryCount; i++) {
        if (registry[i].id == id) {
            LOG_ERROR("Duplicated event is rejected. event id = %s%s%s",
                      LOG_COLOR_RED, id, LOG_COLOR_RESET);
            return false;
        }
    }

    if (registryCount >= EVENT_REGISTRY_CAPACITY) {
        LOG_ERROR("Event registerer capacity is full.");
        return false;
    }

    registry[registryCount].id      = id;
    registry[registryCount].factory = factory;

    registryCount++;

    LOG_TRACE("Event with id = %s%d%s has been registered successfully.",
              LOG_COLOR_RED, id, LOG_COLOR_RESET);

    return true;
}

/* ================= Factory ================= */

Event* createEvent(EventId id) {
    for (uint16_t i = 0; i < registryCount; i++) {
        if (registry[i].id == id) {
            return registry[i].factory();
        }
    }

    STM_LOG("Event not registered: %s%u", LOG_COLOR_RED, (unsigned)id);

    return NULL;
}

void eventInit(void) { registryCount = 0; }

OOP_CTOR(Event) {
    self->target            = NULL;
    self->vtable.dispatch   = dispatch;
    self->vtable.dispatchTo = NULL;
}