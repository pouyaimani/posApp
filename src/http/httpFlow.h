#ifndef HTTP_FLOW_H
#define HTTP_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "httpParser.h"
#include "nth/nth.h"
#include "byteArray.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HTTP methods supported by TMS.
 */
typedef enum {

    HTTP_METHOD_GET = 0,

    HTTP_METHOD_POST,

    HTTP_METHOD_PUT,

    HTTP_METHOD_DELETE

} HttpMethod;

/*
 * HttpFlow runtime state.
 */
typedef enum {

    HTTP_FLOW_IDLE = 0,

    HTTP_FLOW_CONNECTING,

    HTTP_FLOW_BUILDING,

    HTTP_FLOW_SENDING,

    HTTP_FLOW_RECEIVING_HEADERS,

    HTTP_FLOW_RECEIVING_BODY,

    HTTP_FLOW_COMPLETED,

    HTTP_FLOW_FAILED

} HttpFlowState;

/*
 * Result visible to application.
 */
typedef enum {

    HTTP_FLOW_OK = 0,

    HTTP_FLOW_ERR_INVALID_ARG,

    HTTP_FLOW_ERR_BUSY,

    HTTP_FLOW_ERR_NO_RESOURCE,

    HTTP_FLOW_ERR_CONNECT,

    HTTP_FLOW_ERR_SEND,

    HTTP_FLOW_ERR_RECEIVE,

    HTTP_FLOW_ERR_TIMEOUT,

    HTTP_FLOW_ERR_REQUEST,

    HTTP_FLOW_ERR_PARSE,

    HTTP_FLOW_ERR_HTTP_STATUS,

    HTTP_FLOW_ERR_BODY,

    HTTP_FLOW_ERR_INTERNAL

} HttpFlowResult;

typedef struct HttpFlow HttpFlow;

/*
 * Request header.
 *
 * Strings must remain valid during transaction lifetime.
 */
typedef struct {

    const char* name;

    const char* value;

} HttpFlowHeader;

/*
 * HTTP request description.
 */
typedef struct {

    HttpMethod method;

    const char* host;

    uint16_t port;

    const char* path;

    const HttpFlowHeader* headers;

    size_t headerCount;

    const uint8_t* body;

    size_t bodyLength;

} HttpFlowRequest;

/*
 * Result passed when flow finishes.
 */
typedef struct {

    HttpFlowResult result;

    int httpStatus;

    NthResult nthResult;

    HttpParseResult parseResult;

} HttpFlowStatus;

/*
 * Called when HTTP headers are parsed.
 *
 * Example:
 *
 * check Content-Type
 * check server version
 *
 */
typedef void (*HttpFlowHeadersCallback)(HttpFlow*           flow,
                                        const HttpResponse* response);

/*
 * Called for every body chunk.
 *
 * The buffer is owned by HttpFlow/NTH.
 *
 * The callback MUST consume it immediately.
 */
typedef int (*HttpFlowBodyCallback)(HttpFlow* flow, const uint8_t* data,
                                    size_t length);

/*
 * Called when transaction finishes.
 */
typedef void (*HttpFlowDoneCallback)(HttpFlow*             flow,
                                     const HttpFlowStatus* status);

typedef struct {

    HttpFlowHeadersCallback onHeaders;

    HttpFlowBodyCallback onBody;

    HttpFlowDoneCallback onDone;

} HttpFlowCallbacks;

struct HttpFlow {

    /*
     * State owner.
     *
     * Optional RTOS state machine owner.
     */
    State* owner;

    /*
     * Underlying transport transaction.
     */
    NthTransaction* tx;

    /*
     * Current HTTP state.
     */
    HttpFlowState state;

    /*
     * Current request.
     */
    HttpFlowRequest request;

    /*
     * HTTP parser.
     */
    HttpParser parser;

    /*
     * HTTP result information.
     */
    HttpFlowStatus status;

    /*
     * User callbacks.
     */
    HttpFlowCallbacks callbacks;

    /*
     * User context.
     */
    void* userData;

    /*
     * Request build buffer.
     *
     * Usually points into NTH tx buffer.
     */
    ByteArray requestBuffer;

    /*
     * Whether this object owns NTH transaction.
     */
    bool ownsTransaction;
};

/*
 * Initialize HttpFlow object.
 */
void httpFlowInit(HttpFlow* flow);

/*
 * Start HTTP transaction.
 *
 * This function:
 *
 * 1. allocates NTH transaction if needed
 * 2. connects
 * 3. asynchronously sends request
 *
 */
HttpFlowResult httpFlowStart(HttpFlow* flow, State* owner, const char* host,
                             uint16_t port, const HttpFlowRequest* request,
                             const HttpFlowCallbacks* callbacks,
                             void*                    userData);

/*
 * Cancel current HTTP transaction.
 */
void httpFlowCancel(HttpFlow* flow);

/*
 * Release resources.
 */
void httpFlowRelease(HttpFlow* flow);

/*
 * State queries.
 */
bool httpFlowIsComplete(const HttpFlow* flow);

bool httpFlowIsFailed(const HttpFlow* flow);

/*
 * Response information.
 */
const HttpResponse* httpFlowGetResponse(const HttpFlow* flow);

/*
 * Get body received so far.
 *
 * Mainly useful for small responses.
 *
 * Streaming users should use onBody().
 */
const uint8_t* httpFlowGetBody(const HttpFlow* flow);

size_t httpFlowGetBodyLength(const HttpFlow* flow);

#ifdef __cplusplus
}
#endif

#endif