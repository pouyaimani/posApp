#ifndef EVENT_IDS_H
#define EVENT_IDS_H

typedef uint16_t EventId;

#define SM_EVENT_TIME_OUT          ((EventId)0)
#define SM_EVENT_KEYPAD            ((EventId)1)
#define SM_EVENT_MAG               ((EventId)2)
#define SM_EVENT_WIFI              ((EventId)3)
#define SM_EVENT_CELLULAR          ((EventId)4)
#define SM_EVENT_SOCKET_CONNECT    ((EventId)5)
#define SM_EVENT_SOCKET_SENT       ((EventId)6)
#define SM_EVENT_SOCKET_READY_READ ((EventId)7)
#define SM_EVENT_SOCKET_TIME_OUT   ((EventId)8)
#define SM_EVENT_PED               ((EventId)9)
#define SM_EVENT_PICC              ((EventId)10)

#endif