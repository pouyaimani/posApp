#include "states.h"
#include "logger.h"
#include "dev.h"
#include "display.h"
#include "event.h"
#include "assets.h"
#include "ui/ui.h"
#include "eventloop.h"
#include "storage/storage.h"
#include "network/network.h"
#include "transmitter.h"

static Network *net;

static SubState *connecting;
static SubState *receiveData;
static SubState *sendData;
static SubState *processHttp;

static int socketId;

static HttpContext *httpCtx;

static State *onSucess;
static State *onFailure;

/******************** Http helper functions **********************/

static bool httpIsCompleteIdentity()
{
    return httpCtx->bodyReceived >= httpCtx->contentLength;
}

static void httpPrepareRequest()
{
    if (httpCtx->rangeStart > 0) {
        httpCtx->txLen = snprintf((char *)httpCtx->txBuf, 512,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Range: bytes=%u-\r\n"
            "Connection: close\r\n\r\n",
            httpCtx->path,
            httpCtx->host,
            httpCtx->rangeStart);
    } else {
        httpCtx->txLen = snprintf((char *)httpCtx->txBuf, 512,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n\r\n",
            httpCtx->path,
            httpCtx->host);
    }
}

static bool httpParseHeader()
{
    char *end = strstr((char *)httpCtx->rxBuf, "\r\n\r\n");
    if (!end) return false;

    httpCtx->headerSize = (end - (char *)httpCtx->rxBuf) + 4;

    sscanf((char *)httpCtx->rxBuf, "HTTP/%*d.%*d %d", &httpCtx->statusCode);

    // detect body type
    // if (strcasestr((char *)httpCtx->rxBuf, "Transfer-Encoding: chunked")) {
    //     httpCtx->bodyType = HTTP_BODY_CHUNKED;
    // } else {
    //     httpCtx->bodyType = HTTP_BODY_IDENTITY;

    //     char *cl = strcasestr((char *)httpCtx->rxBuf, "Content-Length:");
    //     if (cl) {
    //         sscanf(cl, "Content-Length: %u", &httpCtx->contentLength);
    //     }
    // }

    httpCtx->headerParsed = true;
    return true;
}

static void httpProcessBodyChunk(uint8_t *data, uint32_t len)
{
    if (httpCtx->onBodyChunk) {
        httpCtx->onBodyChunk(data, len);
    }

    httpCtx->bodyReceived += len;
}

static bool parseChunkSize(uint8_t *buf, uint32_t len, uint32_t *size, uint32_t *consumed)
{
    char tmp[16] = {0};

    for (uint32_t i = 0; i < len && i < sizeof(tmp)-1; i++) {
        if (buf[i] == '\r') {
            memcpy(tmp, buf, i);
            *size = (uint32_t)strtoul(tmp, NULL, 16);
            *consumed = i + 2; // \r\n
            return true;
        }
    }
    return false;
}

static void httpProcessChunked(uint8_t *data, uint32_t len)
{
    uint32_t offset = 0;

    while (offset < len) {

        // Step 1: parse chunk size
        if (!httpCtx->chunkHeaderParsed) {
            uint32_t consumed = 0;

            if (!parseChunkSize(data + offset, len - offset,
                                &httpCtx->chunkSize, &consumed)) {
                return; // need more data
            }

            offset += consumed;
            httpCtx->chunkReceived = 0;
            httpCtx->chunkHeaderParsed = true;

            // last chunk
            if (httpCtx->chunkSize == 0) {
                httpCtx->bodyReceived = httpCtx->contentLength;
                return;
            }
        }

        // Step 2: read chunk data
        uint32_t remainingChunk = httpCtx->chunkSize - httpCtx->chunkReceived;
        uint32_t available = len - offset;

        uint32_t toCopy = (remainingChunk < available) ? remainingChunk : available;

        if (toCopy > 0) {
            // processBodyChunk(data + offset, toCopy);
            httpCtx->chunkReceived += toCopy;
            offset += toCopy;
        }

        // Step 3: chunk complete → skip CRLF
        if (httpCtx->chunkReceived == httpCtx->chunkSize) {
            if (len - offset >= 2) {
                offset += 2; // skip \r\n
                httpCtx->chunkHeaderParsed = false;
            } else {
                return; // wait for next packet
            }
        }
    }
}

/******************** Connection sub state **********************/

STATE_DEF_ENTER(Connecting) {
    // connect();
    if (socketId < 0) {
        GOTO_INFO(onFailure, onFailure, "خطا در اتصال", "");
        return;
    }
    SHOW_INFO("در حال اتصال", "");
}

STATE_DEF_EXIT(Connecting) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(Connecting, KeypadEvent) {
    SM_GOTO(sendData);
}

STATE_DEF_HANDLE(Connecting, SocketConnectEvent) {
    if(ev->isConnected) {
        GOTO_INFO(onFailure, onFailure, "خطا در اتصال", "");
    } else {
        SM_GOTO(sendData);
    }
}

static void Connecting(State *parent) {
    connecting = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, connecting, parent, "Connection");
    connecting->vtable.enter = STATE_ENTER(Connecting);
    connecting->vtable.exit = STATE_EXIT(Connecting);
    connecting->vtable.handleKeypad = STATE_HANDLE(Connecting, KeypadEvent);
    connecting->vtable.onSocketConnect = STATE_HANDLE(Connecting, SocketConnectEvent);
}

/******************************************************************/

/******************** Send data sub state **********************/

static void prepareMsg() {

}

STATE_DEF_ENTER(SendData) {
    // SHOW_INFO("HTTP: ارسال", "");

    httpPrepareRequest();

    int ret = net->send(httpCtx->txBuf, httpCtx->txLen);

    if (ret != httpCtx->txLen) {
        GOTO_INFO(onFailure, onFailure, "HTTP Send Error", "");
    }
}

STATE_DEF_EXIT(SendData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(SendData, KeypadEvent) {
    SM_GOTO(receiveData);
}

STATE_DEF_HANDLE(SendData, SocketSentEvent) {
    SHOW_INFO("HTTP: ارسال", "");
    httpPrepareRequest();

    int ret = net->send(httpCtx->txBuf, httpCtx->txLen);

    if (ret != httpCtx->txLen) {
        GOTO_INFO(onFailure, onFailure, "HTTP Send Error", "");
    }
}

static void SendData(State *parent) {
    sendData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, sendData, parent, "Send Data");
    sendData->vtable.enter = STATE_ENTER(SendData);
    sendData->vtable.exit = STATE_EXIT(SendData);
    sendData->vtable.handleKeypad = STATE_HANDLE(SendData, KeypadEvent);
    sendData->vtable.onSocketSent = STATE_HANDLE(SendData, SocketSentEvent);
}

/******************************************************************/

/******************** Receive data sub state **********************/

STATE_DEF_ENTER(ReceiveData) {
    SHOW_INFO("دریافت اطلاعات", "");
}

STATE_DEF_EXIT(ReceiveData) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(ReceiveData, KeypadEvent) {
    GOTO_IDLE();
}

STATE_DEF_HANDLE(ReceiveData, SocketReadyReadEvent)
{
    if (ev->recDataLen <= 0) {
        GOTO_INFO(onFailure, onFailure, "HTTP Recv Error", "");
        return;
    }

    // append to buffer (for header only)
    if (!httpCtx->headerParsed) {
        memcpy(httpCtx->rxBuf + httpCtx->rxLen, ev->recData, ev->recDataLen);
        httpCtx->rxLen += ev->recDataLen;

        if (!httpParseHeader()) return;

        uint32_t bodyOffset = httpCtx->headerSize;
        uint32_t bodyLen = httpCtx->rxLen - bodyOffset;

        if (bodyLen > 0) {
            if (httpCtx->bodyType == HTTP_BODY_CHUNKED) {
                httpProcessChunked(httpCtx->rxBuf + bodyOffset, bodyLen);
            } else {
                // processBodyChunk(httpCtx->rxBuf + bodyOffset, bodyLen);
            }
        }

        return;
    }

    // body phase
    if (httpCtx->bodyType == HTTP_BODY_CHUNKED) {
        httpProcessChunked(ev->recData, ev->recDataLen);
    } else {
        // processBodyChunk(ev->recData, ev->recDataLen);
    }

    // completion
    if (httpCtx->bodyType == HTTP_BODY_IDENTITY &&
        httpIsCompleteIdentity())
    {
        SM_GOTO(processHttp);
    }

    if (httpCtx->bodyType == HTTP_BODY_CHUNKED &&
        httpCtx->chunkSize == 0)
    {
        SM_GOTO(processHttp);
    }
}

static void ReceiveData(State *parent) {
    receiveData = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, receiveData, parent, "ReceiveData");
    receiveData->vtable.enter = STATE_ENTER(ReceiveData);
    receiveData->vtable.exit = STATE_EXIT(ReceiveData);
    receiveData->vtable.handleKeypad = STATE_HANDLE(ReceiveData, KeypadEvent);
    receiveData->vtable.onSocketReadyRead = STATE_HANDLE(ReceiveData, SocketReadyReadEvent);
}

/******************** Process sub state **********************/

STATE_DEF_ENTER(ProcessHttp) {
    SHOW_INFO("پردازش HTTP", "");

    // LOG_D("HTTP Status: %d", httpCtx->statusCode);

    if (httpCtx->statusCode != 200 &&
        httpCtx->statusCode != 206)
    {
        GOTO_INFO(onFailure, onFailure, "HTTP Error", "");
        return;
    }

    // 👉 here pass to TMS or upper layer

    SM_GOTO(onSucess);
}

STATE_DEF_EXIT(ProcessHttp) {
    HIDE_INFO();
}

STATE_DEF_HANDLE(ProcessHttp, KeypadEvent) {
    GOTO_IDLE();
}

static void ProcessHttp(State *parent) {
    processHttp = (SubState *)GET_MEM(sizeof(SubState));
    OOP_CALL_CTOR(State, processHttp, parent, "process http");
    processHttp->vtable.enter = STATE_ENTER(ProcessHttp);
    processHttp->vtable.exit = STATE_EXIT(ProcessHttp);
    processHttp->vtable.handleKeypad = STATE_HANDLE(ProcessHttp, KeypadEvent);
}

/******************************************************************/

STATE_DEF_ENTER(HttpTransmitter) {
    SM_GOTO(connecting);
}

OOP_CTOR(HttpTransmitter, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(HttpTransmitter);

    self->httpCtx = GET_MEM(sizeof(HttpContext));
    httpCtx = self->httpCtx;

    onSucess = self->onSucess;
    onFailure = self->onFailure;

    Connecting(self);
    SendData(self);
    ReceiveData(self);
    ProcessHttp(self);

    net = getNetwork();
}