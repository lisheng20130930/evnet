/*
 * evnetfunclib.h 
 *
 * The lower layer API for tcp and socket level operations,
 * which extensions such as httpc/httpd are based on.
 *
 * Copyright (C) Listen.Li, 2018
 *
 */
#ifndef _EVFUNCLIB_H_
#define _EVFUNCLIB_H_

#include "dataqueue.h"


#define  IPSTRSIZE   (128)

/* channel message */
#define _EVDATA       (1)
#define _EVCLOSED     (2)
#define _EVSENT       (3)

typedef struct msgChannel_s{
    int identify;
    void* channel;
    union{
        int errcode;
        dataqueue_t *dataqueue;
		int size;
    }u;
}msgChannel_t;


/* acceptor message */
#define _EVACCEPTED  (1)
typedef struct msgAcceptor_s{
    int identify;
    union{
        void* channel;
    }u;
}msgAcceptor_t;


typedef int (*pfn_msg_handler)(void *pUser, void *msg, unsigned int size);


void* evnet_createchannel(char *ipStr, unsigned short port, int security);
void evnet_closechannel(void* c, int errcode);
int evnet_channelbind(void* c, pfn_msg_handler handler, unsigned int timeouts, void *pUser);
void* evnet_channeluser(void* c);
bool evnet_channelsend(void* c, char *data, int size);
char* evnet_channelip(void* c);
void* evnet_createacceptor(unsigned short port, int secrity, char *cert, pfn_msg_handler handler, void *pUsr);
void evnet_destroyacceptor(void* acceptor);
int evnet_acceptorstart(void* acceptor);
void evnet_acceptorstop(void* acceptor);
void evnet_hostbyname(char *name, char ipStr[], int len);
int evnet_init(int size);
void evnet_uint();
void evnet_loop(unsigned int loops);
unsigned short evnet_channelport(void* c);


#endif
