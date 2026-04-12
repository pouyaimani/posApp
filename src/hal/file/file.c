#include "file.h"

File *__file;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/file_t3Rtos.h"

static void constructT3Rtos() {
    static FileT3Rtos obj;
    __file = (File *)&obj;
    OOP_CALL_CTOR(File, __file);
    OOP_CALL_CTOR(FileT3Rtos, &obj);
}

#endif

OOP_CTOR(File) {
}

File *file() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return __file;
}