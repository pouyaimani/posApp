#include "byteArray.h"
#include "error.h"
#include "sys/sys.h"

int8_t byteArrayInit(ByteArray *ba, size_t size) {
    RETURN_VALUE_IF_NULL(ba, ;, ERR_NULL_PARAMETER);
    ba->data = MEM_ALLOC(size);
    RETURN_VALUE_IF_NULL(ba->data, ;, ERR_NULL_PARAMETER);
    memset(ba->data, 0, size);
    ba->capacity = size;
    return ERR_OK;
}