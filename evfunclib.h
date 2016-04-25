#ifndef _EVFUNCLIB_H_
#define _EVFUNCLIB_H_

#include "dataqueue.h"

/* channel message */
#define _EVDATA       (1)
#define _EVCLOSED     (2)
typedef struct msgChannel_s
{
    int identify;
    void* channel;
    union{
        int errcode;
        dataqueue_t *dataqueue;
    }u;
}msgChannel_t;


/* acceptor message */
#define _EVACCEPTED  (1)
typedef struct msgAcceptor_s
{
    int identify;
    union{
        void* channel;
    }u;
}msgAcceptor_t;


typedef int (*pfn_msg_handler)(void *pUser, void *msg, unsigned int size);


void* evnet_createchannel(unsigned int ip, unsigned short port);
void evnet_closechannel(void* c, int errcode);
int evnet_channelbind(void* c, pfn_msg_handler handler, unsigned int timeouts, void *pUser);
void* evnet_channeluser(void* c);
bool evnet_channelsend(void* c, char *data, int size);
unsigned int evnet_channelip(void* c);
void* evnet_createacceptor(unsigned short port, pfn_msg_handler handler, void *pUsr);
void evnet_destroyacceptor(void* acceptor);
int evnet_acceptorstart(void* acceptor);
void evnet_acceptorstop(void* acceptor);
unsigned int evnet_hostbyname(char *name);
int evnet_init();
void evnet_uint();
void evnet_loop(unsigned int loops);



#endif
