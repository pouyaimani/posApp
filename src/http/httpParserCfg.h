#ifndef HTTP_CONFIG_H
#define HTTP_CONFIG_H

/*
 * Maximum number of headers exposed to the application.
 */
#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 16U
#endif

#ifndef HTTP_MAX_METHOD_LEN
#define HTTP_MAX_METHOD_LEN 16U
#endif

#ifndef HTTP_MAX_TARGET_LEN
#define HTTP_MAX_TARGET_LEN 256U
#endif

#ifndef HTTP_MAX_HEADER_NAME_LEN
#define HTTP_MAX_HEADER_NAME_LEN 64U
#endif

#ifndef HTTP_MAX_HEADER_VALUE_LEN
#define HTTP_MAX_HEADER_VALUE_LEN 256U
#endif

/*
 * Maximum complete HTTP header section, including the final CRLF.
 */
#ifndef HTTP_MAX_HEADER_BYTES
#define HTTP_MAX_HEADER_BYTES 2048U
#endif

/*
 * Maximum decoded/body payload.
 *
 * Downloader range size is 4096 bytes.
 */
#ifndef HTTP_MAX_BODY_SIZE
#define HTTP_MAX_BODY_SIZE 4096U
#endif

#endif