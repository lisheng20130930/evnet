#include "evfunclib.h"
#include "aesocket.h"
#include "channel.h"
#include "acceptor.h"
#include "event.h"
#include "libnet.h"

static fd_t _listen_sock(unsigned short port)
{
    fd_t listenfd = _INVALIDFD;    
    
    if(_INVALIDFD==(listenfd=aesoccreate(1))){
        return _INVALIDFD;
    }    
    if(aesoclisten(listenfd,port)){
        aesocclose(listenfd);
        return _INVALIDFD;
    }

    return listenfd;
}

acceptor_t* acceptor_create(unsigned short port, pfn_msg_handler handler, void *pUsr)
{
    acceptor_t *acceptor = NULL;
    fd_t fd = _INVALIDFD;
    
    acceptor = (acceptor_t*)malloc(sizeof(acceptor_t));	
    memset(acceptor, 0x00, sizeof(acceptor_t));
    fd = _listen_sock(port);
    if(_INVALIDFD==fd){
        free(acceptor);
        return NULL;
    }        
    acceptor->fd = fd;
    acceptor->handler = handler;
    acceptor->pUsr = pUsr;
    
    return acceptor;
}

void acceptor_destroy(acceptor_t *acceptor)
{
    if(_INVALIDFD!=acceptor->fd){
        aesocclose(acceptor->fd);
        acceptor->fd = _INVALIDFD;
    }
    free(acceptor);
}


static void _notify_channel(acceptor_t *acceptor, channel_t *channel)
{
    msgAcceptor_t msg = {0};
    
    memset(&msg, 0x00, sizeof(msgAcceptor_t));
    msg.identify = _EVACCEPTED;
    msg.u.channel = (void*)channel;
    
    acceptor->handler(acceptor->pUsr, (void*)&msg, sizeof(msgAcceptor_t));
}

static void _accept_handler(aeEventLoop *el, fd_t fd, void *privdata, int mask)
{
    acceptor_t *acceptor = (acceptor_t*)privdata;
    fd_t accepted = _INVALIDFD;
    unsigned int ip = 0;
    unsigned short port = 0;
    channel_t *channel = NULL;
   
    if(0!=aesocaccept(acceptor->fd, &accepted, &ip, &port)){
        return;
    }
    
    channel = channel_create(accepted, ip, port, 0);
    if(NULL==channel){
        aesocclose(accepted);
        return;
    }
    
    _notify_channel(acceptor, channel);
}

int acceptor_start(acceptor_t *acceptor)
{
    if(0!=aeCreateFileEvent(g_libnet.evLoop,acceptor->fd,
        AE_READABLE,_accept_handler,(void*)acceptor)){
        return (-1);
    }
    return 0;
}

void acceptor_stop(acceptor_t *acceptor)
{
    aeDeleteFileEvent(g_libnet.evLoop, acceptor->fd, AE_READABLE);
}
