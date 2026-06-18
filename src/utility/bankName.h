#ifndef BANKNAME_H_
#define BANKNAME_H_

#include "oop.h"

OOP_CLASS(BankName) {
    OOP_METHOD(void, getNameFa, const char* iin, char* out, size_t size);
    OOP_METHOD(void, getNameEn, const char* iin, char* out, size_t size);
};

OOP_CTOR(BankName);

BankName* bankName();

#endif