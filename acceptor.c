#include "evfunclib.h"
#include "aesocket.h"
#include "channel.h"
#include "acceptor.h"
#include "event.h"
#include "libnet.h"


#ifdef SSL_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>

static __inline int _SSL_do_Key_Cert(SSL_CTX *sslCtx, char *cert)
{
	int ret = 0;
	ret = SSL_CTX_use_certificate_file(sslCtx, cert, SSL_FILETYPE_PEM);
	if(ret!=1){
		return -1;
	}
	ret = SSL_CTX_use_PrivateKey_file(sslCtx, cert, SSL_FILETYPE_PEM);
	if(ret!=1){
		return -1;
	}
	ret = SSL_CTX_check_private_key(sslCtx);
	if(ret!=1){
		return -1;
	}
	return 0;
}
#endif


static fd_t _listen_sock(int family, unsigned short port)
{
    fd_t listenfd = _INVALIDFD;    
    
    if(_INVALIDFD==(listenfd=aesoccreate(family, 1))){
        return _INVALIDFD;
    }    
    if(aesoclisten(listenfd,port)){
        aesocclose(listenfd);
        return _INVALIDFD;
    }

    return listenfd;
}

acceptor_t* acceptor_create(int family, unsigned short port, int secrity, char *cert, pfn_msg_handler handler, void *pUsr)
{
    acceptor_t *acceptor = NULL;
    fd_t fd = _INVALIDFD;
    
    acceptor = (acceptor_t*)malloc(sizeof(acceptor_t));	
    memset(acceptor, 0x00, sizeof(acceptor_t));
    fd = _listen_sock(family,port);
    if(_INVALIDFD==fd){
        free(acceptor);
        return NULL;
    }        
    acceptor->fd = fd;
    acceptor->handler = handler;
    acceptor->pUsr = pUsr;

	#ifdef SSL_SUPPORT
	if(!secrity){ /* NOT SSL accecptor */
		return acceptor;
	}	
	SSL_CTX *sslCtx = SSL_CTX_new(TLSv1_method());
	if(_SSL_do_Key_Cert(sslCtx,cert)){
		SSL_CTX_free(sslCtx);
		free(acceptor);
		return NULL;
	}
	acceptor->secrity = secrity;	
	acceptor->ctx = sslCtx;
	#endif
    
    return acceptor;
}

void acceptor_destroy(acceptor_t *acceptor)
{
	#ifdef SSL_SUPPORT
	if(NULL!=acceptor->ctx){
		SSL_CTX_free((SSL_CTX*)acceptor->ctx);
		acceptor->ctx = NULL;
		acceptor->secrity = 0;
	}
	#endif    
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
    char ipStr[IPSTRSIZE] = {0};
    unsigned short port = 0;
    channel_t *channel = NULL;
	int secrity = 0;
   
    if(0!=aesocaccept(acceptor->fd, &accepted, ipStr, IPSTRSIZE, &port)){
        return;
    }
    
	#ifdef SSL_SUPPORT
	secrity = acceptor->secrity;
	#endif

    channel = channel_create(accepted, ipStr, port, 0, secrity);
    if(NULL==channel){
        aesocclose(accepted);
        return;
    }
    
	#ifdef SSL_SUPPORT
	// assign the SSL ctx to own channel
	channel->ctx = acceptor->ctx;
	#endif

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
