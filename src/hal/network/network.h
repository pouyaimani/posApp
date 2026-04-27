#ifndef NETWORK_H_
#define NETWORK_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"

typedef enum {
    NET_ERR_OK = 0,
    NET_ERR_INPUT_ERR
} NetError_t;

typedef enum {
    NET_ROUTE_CELLUALR = 0,
    NET_ROUTE_WIFI,
    NET_ROUTE_ETH
} NetRoute_t;

typedef enum {
    NET_STATUS_CONNECTING = 0,
    NET_STATUS_DISCONNECTED,
    NET_STATUS_CONNECTED,
    NET_STATUS_ERROR
} SocketStatus_t;

typedef enum {
    NET_AF_INET = 0,                 
    NET_AF_INET6,                    
    NET_AF_UNIX,                     
    NET_AF_ISO,                      
    NET_AF_NS,                       
    NET_AF_IPX,                      
    NET_AF_APPLETALK,                
    NET_AF_ROUTE,                    
    NET_AF_KEY,                      
} SocketFamily_t;

typedef struct {
    SocketFamily_t family;
    char ip[64];
    uint16_t port;
} SocketAddr_t;

typedef enum  {
    NET_STREAM,                      
    NET_DGRAM,                       
    NET_SEQPACKET,                   
    NET_RAW,                         
} SocketType_t;

OOP_DECLARE_CLASS(Network)

OOP_VTABLE(Network) {
    OOP_IMETHOD(NetError_t, Network, init);
    OOP_IMETHOD(NetRoute_t, Network, getRoute);
    OOP_IMETHOD(NetError_t, Network, setRoute, NetRoute_t);
    OOP_IMETHOD(NetError_t, Network, setAddr, const char *, uint16_t);
    OOP_IMETHOD(const SocketAddr_t *, Network, getAddr);
    OOP_IMETHOD(NetError_t, Network, create, SocketAddr_t *, SocketType_t);
    OOP_IMETHOD(NetError_t, Network, close, int);
    OOP_IMETHOD(SocketStatus_t, Network, getStatus, int32_t);
    OOP_IMETHOD(int32_t, Network, send, int32_t socketID, const uint8_t *data, uint32_t dataLen, uint32_t timeOut);
    OOP_IMETHOD(int32_t, Network, receive, int32_t socketID, uint8_t *data, uint32_t dataLen);
};

OOP_CLASS(Network) {
    OOP_IMPLEMENTS(Network);
    SocketAddr_t address;
    OOP_METHOD(int, connect);
    OOP_METHOD(void, disconnect);
    OOP_METHOD(NetError_t, init);
    OOP_METHOD(int, send, uint8_t *data, size_t len);
    int id;
};

OOP_CTOR(Network);

Network *network(void);

#endif