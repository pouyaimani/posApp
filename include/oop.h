#ifndef OOP_H_
#define OOP_H_

#include <stddef.h>

/* ===== Class declaration ===== */

#define OOP_DECLARE_CLASS(name) \
    typedef struct name name;

#define OOP_CLASS(name) \
    typedef struct name name; \
    struct name

/* ===== Base / inheritance ===== */

#define OOP_EXTENDS(base_) \
    base_ base

/* ===== Methods ===== */

#define OOP_METHOD(ret_type, name, ...) \
    ret_type (*name)(##__VA_ARGS__)

#define OOP_IMETHOD(ret_type, type, name, ...) \
    ret_type (*name)(type* self, ##__VA_ARGS__)

/* ===== Constructors / destructors ===== */

#define OOP_CTOR(type, ...) \
    void type##_ctor(type* self, ##__VA_ARGS__)

#define OOP_DTOR(type) \
    void type##_dtor(type* self)

/* ===== Interface (vtable) ===== */

#define OOP_VTABLE(name) \
    typedef struct name##VTable name##VTable; \
    struct name##VTable

#define OOP_IMPLEMENTS(name) \
    const name##VTable* vtable

/* ===== Virtual call ===== */

#define OOP_CALL(obj, method, ...) \
    ((obj)->vtable->method((obj), ##__VA_ARGS__))

/* ===== container_of (safe inheritance cast) ===== */

#define OOP_CONTAINER_OF(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))


#define CALL_ONCE(code)        \
    do {                       \
        static int _called;    \
        if (!_called) {        \
            _called = 1;       \
            code;              \
        }                      \
    } while (0)

#endif