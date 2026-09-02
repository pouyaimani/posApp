#ifndef HTTP_CONFIG_H
#define HTTP_CONFIG_H

#include "nth/nth.h"

/*
 * Maximum number of HTTP response/request headers that the parser
 * will expose to the application.
 */
#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 16U
#endif

/*
 * Maximum HTTP method length.
 *
 * GET       = 3
 * POST      = 4
 * DELETE    = 6
 * OPTIONS   = 7
 * CONNECT   = 7
 */
#ifndef HTTP_MAX_METHOD_LEN
#define HTTP_MAX_METHOD_LEN 16U
#endif

/*
 * Maximum request-target / response reason phrase length
 * that the application is interested in.
 *
 * This does NOT allocate a buffer of this size.
 */
#ifndef HTTP_MAX_TARGET_LEN
#define HTTP_MAX_TARGET_LEN 256U
#endif

/*
 * Maximum header name/value lengths that the application accepts.
 *
 * Again, these are validation limits rather than allocations.
 */
#ifndef HTTP_MAX_HEADER_NAME_LEN
#define HTTP_MAX_HEADER_NAME_LEN 64U
#endif

#ifndef HTTP_MAX_HEADER_VALUE_LEN
#define HTTP_MAX_HEADER_VALUE_LEN 256U
#endif

/*
 * Maximum HTTP body size that the HTTP layer is willing to process.
 *
 * This is an application/protocol limit and should normally be
 * smaller than or equal to NT_BUFFER_SIZE.
 */
#ifndef HTTP_MAX_BODY_SIZE
#define HTTP_MAX_BODY_SIZE NT_BUFFER_SIZE
#endif

#endif /* HTTP_CONFIG_H */