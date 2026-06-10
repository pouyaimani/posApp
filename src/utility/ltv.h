#ifndef LTV_H_
#define LTV_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct {
	int len;
	char tag[4];
	char data[256];
} LtvStructInfo;


void setCommonLtv(char *deviceSerial, const char *version, int iLang, char *buff);

int unpackLtv(char *buffer, LtvStructInfo ltvStructInfo[]);

#endif