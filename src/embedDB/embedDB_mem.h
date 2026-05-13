#ifndef EMBEDDB_MEM_H_
#define EMBEDDB_MEM_H_

#include "sys/sys.h"

#define EMDB_MEM_ALLOC(size)            GET_MEM(size)
#define EMDB_MEM_FREE(size)             FREE_MEM(size)

#ifndef EMDB_MEM_ALLOC
    #error EmbedDB: memory allocator is not defined.
#endif

#ifndef EMDB_MEM_FREE
    #error EmbedDB: memory releaser is not defined.
#endif

#endif