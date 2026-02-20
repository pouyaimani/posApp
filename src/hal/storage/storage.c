#include "storage.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"

Storage *__storage;
DevSettings _settings;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/storage_t3Rtos.h"

static void constructT3Rtos() {
    static StorageT3Rtos obj;
    __storage = (Storage *)&obj;
    OOP_CALL_CTOR(Storage, __storage);
    OOP_CALL_CTOR(StorageT3Rtos, &obj);
}

#endif

OOP_CTOR(Storage) {
    self->settings = &_settings;
}

Storage *getStorage() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce storage is undefined. Make sure correct device is chosen and its storage driver is developed.
#endif
    );
    return __storage;
}