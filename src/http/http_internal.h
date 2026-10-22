#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H
#include "http.h"
bool http_token(HttpView v);
bool http_field_value(HttpView v);
HttpView http_trim(HttpView v);
HttpResult http_decimal(HttpView v, uint64_t *value);
#endif
