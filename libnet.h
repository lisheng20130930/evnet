#ifndef _LIBNET_H_
#define _LIBNET_H_

#include "evfunclib.h"

#define _BUFF_LEN  (32*1024)

typedef struct libnet_s
{
    channel_t  *channelHead;
    aeEventLoop *evLoop;
    char _buff[_BUFF_LEN];
}libnet_t;

extern libnet_t g_libnet;

#endif
