#if !defined(DESKTOP_FILE_INTERFACE)
#define DESKTOP_FILE_INTERFACE

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

#if defined(DIST)
#include "embedDB.h"
#else
#include "embedDB/embedDB.h"
#endif

/* File functions */
embedDBFileInterface* getFileInterface();

void* setupFile(const char* filename);
void  tearDownFile(void* file);

#ifdef __cplusplus
}
#endif

#endif