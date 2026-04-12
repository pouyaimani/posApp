#ifndef FILE_H_
#define FILE_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FILE_ERR_NONE,                     
    FILE_ERR_CRCERR,                   
    FILE_ERROR,                    
    FILE_SEEK_ERROR,               
    FILE_ERR_EOF,                      
    FILE_ERR_OK                      
} FileErr_t;

typedef enum {
    FILE_OPEN_R,     
    FILE_OPEN_W,     
    FILE_OPEN_A,     

    FILE_OPEN_RR,    
    FILE_OPEN_WW,    
    FILE_OPEN_AA,    
} FileOpenMode_t;

OOP_DECLARE_CLASS(File)

OOP_VTABLE(File) {
    OOP_IMETHOD(bool, File, isExist, char *);
    OOP_IMETHOD(FileErr_t, File, create, char *, uint32_t, uint8_t);
    OOP_IMETHOD(FileErr_t, File, read, char *, uint8_t *, uint32_t, uint32_t *);
    OOP_IMETHOD(FileErr_t, File, write, char *, uint8_t *, uint32_t);
    OOP_IMETHOD(FileErr_t, File, append, char *, uint8_t *, uint32_t);
    OOP_IMETHOD(FileErr_t, File, insert, char *, uint8_t *, uint32_t, uint32_t);
    OOP_IMETHOD(FileErr_t, File, delete, char *);
    OOP_IMETHOD(uint32_t, File, getSize, char *);
    OOP_IMETHOD(uint32_t, File, getFreeSpace, char *);
};

OOP_CLASS(File) {
    OOP_IMPLEMENTS(File);
};

OOP_CTOR(File);

File *file(void);

#endif