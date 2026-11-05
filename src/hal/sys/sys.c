#include "sys.h"

// #define SYS_MEM_LOG 1

System*     __sys;
BatteryStat batterySt;
char        serialNumber[SERIAL_NUMBER_MAX_LEN];

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/sys_t3Rtos.h"

static void constructT3Rtos() {
    static T3Rtos obj;
    __sys = (System*)&obj;
    OOP_CALL_CTOR(System, __sys, "");
    OOP_CALL_CTOR(T3Rtos, &obj, "");
}

#endif

void* sys_getMemory_debug(size_t size, const char* file, int line,
                          const char* func) {
    void* ptr;
#ifdef SYS_MEM_LOG
    LOG_TRACE(
        "########## [ALLOC] size = %s%u%s bytes from %s:%d (%s) ##########",
        LOG_COLOR_ERROR, (unsigned)size, LOG_COLOR_RESET, file, line, func);
#endif

    ptr = OOP_CALL(sys(), getMemory, size);

    if (ptr) {
        memset(ptr, 0, size);
    }

#ifdef SYS_MEM_LOG
    LOG_TRACE("########## address=%s%p%s ##########", LOG_COLOR_ERROR, ptr,
              LOG_COLOR_RESET);
#endif

    return ptr;
}

void sys_freeMemory_debug(void* mem, const char* file, int line,
                          const char* func) {
#ifdef SYS_MEM_LOG
    LOG_TRACE("########## [FREE] addr= %s%p%s bytes from %s:%d (%s) ##########",
              LOG_COLOR_ERROR, mem, LOG_COLOR_RESET, file, line, func);
#endif
    OOP_CALL(sys(), freeMemory, mem);
}

OOP_CTOR(System, const char* name) {
    self->name = name;
    memset(&self->vtable, 0, sizeof(self->vtable));
}

System* sys() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Deivce is undefined. Make sure correct device is chosen and developed.
#endif
    );
    return __sys;
}