#ifndef LTV_H_
#define LTV_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int  len;
    char tag[4];
    char data[256];
} LtvStructInfo;

void setCommonLtv(char* deviceSerial, const char* version, int iLang,
                  char* buff);

void setTopupLtv(char* deviceSerial, const char* version, int iLang,
                 const char* code, const char* phone, char* buff);
void setVoucherLtv(char* deviceSerial, const char* version, int iLang,
                   const char* code, char* buff);

int unpackLtv(char* buffer, LtvStructInfo ltvStructInfo[]);

#endif