#include "evfunclib.h"
#include "aesocket.h"
#include "channel.h"
#include "event.h"
#include "libnet.h"
#include "time.h"


#define _CHANNEL_LOOPS_NUM    (80)
#define _MAX_CONNECTING_SECS  (3)
#define _HTONL(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff000000)>>24)|(((x)&0xff0000)>>8))
#define _FLG_RECV_ENABLED    (0x01)
#define _FLG_SEND_ENABLED    (0x02)


#ifdef SSL_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Win32 load openSSL libs */
#ifdef WIN32
#define EV_FD_TO_WIN32_HANDLE(fd) _get_osfhandle(fd)
#pragma comment (lib,"libeay32.lib")
#pragma comment (lib,"ssleay32.lib")
#endif


static __inline int _SSL_write(void *ssl, char *pszBuff, int iSize)
{
	int ret = SSL_write((SSL*)ssl,pszBuff,iSize);
	if(ret<=0){
		if(SSL_ERROR_WANT_WRITE!=SSL_get_error((SSL*)ssl,ret)){
            return (-1);
        }
		return 0;
	}
	return (0>=ret)?0:ret;
}

static __inline int _SSL_read(void *ssl, char *pszBuff, int iSize)
{
	int ret = SSL_read((SSL*)ssl,pszBuff,iSize);
	if(ret<=0){
		if(SSL_ERROR_WANT_READ!=SSL_get_error((SSL*)ssl,ret)){			
            return (-1);
        }		
		return 0;
	}
	return (0>=ret)?0:ret;
}

static __inline int _SSL_do_handshake(void *ssl)
{
	int ret = SSL_do_handshake((SSL*)ssl);
	if(ret==1){ // SUCCESS
		return 0;
	}
	ret = SSL_get_error((SSL*)ssl,ret);
	if(ret==SSL_ERROR_WANT_WRITE||ret==SSL_ERROR_WANT_READ){
		return _wouldblock;
	}
	return -1;
}

void Evnet_initSSL()
{
	SSL_load_error_strings();
	SSL_library_init();
	SSLeay_add_ssl_algorithms();
}

#endif


static void _write_callback(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask);
static void _read_callback(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask);


channel_t* channel_create(fd_t fd, char* ipStr, unsigned short port, int upstream, int security)
{
    channel_t *channel = NULL;

    channel = (channel_t*)malloc(sizeof(channel_t));
    if(!channel){
        return NULL;
    }
    memset(channel, 0x00, sizeof(channel_t));
    channel->fd = fd;
    strcpy(channel->ipStr,ipStr);
    channel->port = port;
    channel->upstream = upstream;
	#ifdef SSL_SUPPORT
	channel->security = security;
	#endif

    if(g_libnet.channelHead){
        channel->next = g_libnet.channelHead;
        channel->prev = g_libnet.channelHead->prev;
        g_libnet.channelHead->prev->next = channel;
        g_libnet.channelHead->prev = channel;
    }
    else{
        channel->next = channel;
        channel->prev = channel;
    }
    g_libnet.channelHead = channel;

    return channel;
}

void channel_close(channel_t *channel, int errcode)
{
    pfn_msg_handler handler = channel->handler;
    void *pUsr = channel->pUsr;
    msgChannel_t msg = {0};
    
    if(channel == g_libnet.channelHead){
        g_libnet.channelHead = g_libnet.channelHead->next;
    }
    if(channel == g_libnet.channelHead){
        g_libnet.channelHead = NULL;
    }
    else{
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
    }
    channel->next = NULL;
    channel->prev = NULL;

	#ifdef SSL_SUPPORT
	/* free ssl */
	if(NULL!=channel->ssl){
		SSL_shutdown((SSL*)channel->ssl);
		SSL_free((SSL*)channel->ssl);
		channel->ssl = NULL;
	}
	/* free upstream SSL ctx */
	if(channel->upstream&&NULL!=channel->ctx){
		SSL_CTX_free((SSL_CTX*)channel->ctx);
		channel->ctx = NULL;
	}
	#endif
    /* notify channel close if has been binded */
    if(NULL != handler){
        memset(&msg, 0x00, sizeof(msgChannel_t));
        msg.identify = _EVCLOSED;
        msg.channel = channel;
        msg.u.errcode = errcode;
        handler(pUsr,(void*)&msg,sizeof(msgChannel_t));
    }
    
    /* clear */
    if(channel->flg & _FLG_RECV_ENABLED){
        aeDeleteFileEvent(g_libnet.evLoop, channel->fd, AE_READABLE);
        channel->flg &= ~_FLG_RECV_ENABLED;
    }        
    
    if(channel->flg & _FLG_SEND_ENABLED){
        aeDeleteFileEvent(g_libnet.evLoop, channel->fd, AE_WRITABLE);
        channel->flg &= ~_FLG_SEND_ENABLED;
    }

    dataqueue_uinit(&channel->inDataqueue);
    dataqueue_uinit(&channel->outDataqueue);
    
    if(_INVALIDFD != channel->fd){        
        aesocclose(channel->fd);
        channel->fd = _INVALIDFD;
    }

    free(channel);
}


static bool channel_handshake(struct aeEventLoop *eventLoop, channel_t *channel)
{
	#ifdef SSL_SUPPORT
	if(channel->security){
		// when socket conneted, we setup SSL
		/* upstream setup ssl ctx */
		if(channel->upstream&&NULL==channel->ctx){
			channel->ctx = SSL_CTX_new(TLSv1_method());
		}
		/* setup SSL */
		if(NULL==channel->ssl){
			channel->ssl = SSL_new((SSL_CTX*)channel->ctx);
			SSL_set_mode((SSL*)channel->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
			#ifdef WIN32
			SSL_set_fd((SSL*)channel->ssl,EV_FD_TO_WIN32_HANDLE(channel->fd));
			#else
			SSL_set_fd((SSL*)channel->ssl,channel->fd);
			#endif
			if(channel->upstream){
				SSL_set_connect_state((SSL*)channel->ssl);
			}else{
				SSL_set_accept_state((SSL*)channel->ssl);
			}
		}
		
		/* handle handshake */
		int ret = _SSL_do_handshake(channel->ssl);
		if(0!=ret){
			if(_wouldblock!=ret){
				channel_close(channel, (-1));
			}
			return false;
		}		
	}
	#endif
	
	if(channel->upstream){
		if(0==(channel->flg&_FLG_RECV_ENABLED)){
			/* enable read */
			if(0!=aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_READABLE,_read_callback,(void*)channel)){
				channel_close(channel, (-1));
				return false;
			}
			channel->flg |= _FLG_RECV_ENABLED;
		}
	}
	channel->connected = 1; //CONNECTED
	return true;
}

static void _read_callback(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask)
{
    channel_t *channel = (channel_t*)clientdata;
    pfn_msg_handler handler = channel->handler;
    void *pUsr = channel->pUsr;
    msgChannel_t msg = {0};
    int size = 0;

	if(!channel->connected){
		if(!channel->upstream){
			if(!channel_handshake(eventLoop,channel)){
				return;
			}
		}else{
			return;//waiting handeshake(upstream)
		}
    }
    
    /* recv data to data queue */
	#ifdef SSL_SUPPORT
	if(channel->security){		
		size = _SSL_read(channel->ssl, g_libnet._buff, _BUFF_LEN);
		if(size==0){ //SSL can read 0
			return;
		}
    }else //{}//CHK
	#endif
	size = aesocread(channel->fd, g_libnet._buff, _BUFF_LEN);
    if(size <= 0){
        channel_close(channel, (-1));
        return;
    }
    
    /* update last interaction time */
    channel->lastinteraction = time(NULL);
    
    /* write to dataqueue */
    dataqueue_insert_data(&channel->inDataqueue, g_libnet._buff, size);
    
    /* notify out */
    memset(&msg, 0x00, sizeof(msgChannel_t));
    msg.identify = _EVDATA;
    msg.channel = channel;
    msg.u.dataqueue = &channel->inDataqueue;
    handler(pUsr, (void*)&msg, sizeof(msgChannel_t));
}

static void _write_callback(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask)
{
    channel_t *channel = (channel_t*)clientdata;
	pfn_msg_handler handler = channel->handler;
    void *pUsr = channel->pUsr;
    char *buff = NULL;
    int buff_len = 0;
    int size = 0;
    
	if(!channel->connected){
        if(channel->upstream){
            if(!channel_handshake(eventLoop,channel)){
				return;
            }
        }else{
			return; //waiting handeshake(accecpt)
        }
    }
    
    if(0 < dataqueue_datasize(&channel->outDataqueue)){
        /* next data */
        dataqueue_next_data(&channel->outDataqueue, &buff, &buff_len);
		#ifdef SSL_SUPPORT
		if(channel->security){			
			size = _SSL_write(channel->ssl, buff, buff_len);	
			if(size==0){ //SSL can write 0
				return;
			}
		}else //{}//CHK
		#endif
		size = aesocwrite(channel->fd, buff, buff_len);
        if(size <= 0){
			channel_close(channel, (-1));
            return;
        }
        
        channel->lastinteraction = time(NULL);
        dataqueue_distill_data(&channel->outDataqueue, size);
    }
		    
    /* check has data to send */
    if(0>=dataqueue_datasize(&channel->outDataqueue)){
        if(channel->flg & _FLG_SEND_ENABLED){
			aeDeleteFileEvent(g_libnet.evLoop, channel->fd, AE_WRITABLE);
			channel->flg &= ~_FLG_SEND_ENABLED;
		}
    }
      
    // notify sent Msg
    if(size>0){
		msgChannel_t msg = {0};
		memset(&msg, 0x00, sizeof(msgChannel_t));
		msg.identify = _EVSENT;
		msg.channel = channel;
		msg.u.size = size;
		handler(pUsr, (void*)&msg, sizeof(msgChannel_t));
    }
}

int channel_bind(channel_t *channel, pfn_msg_handler handler, unsigned int timeouts, void *pUsr)
{
    int erroroccours = 0;
    
    do{
        if(!channel->upstream){
            /* enable read */
            if(0!=aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_READABLE, _read_callback,(void*)channel)){
                erroroccours = 1;
                break;
            }
            channel->flg |= _FLG_RECV_ENABLED;
            break;
        }
        
        /* connect */
        if (0!=aesocconnect(channel->fd, (char*)channel->ipStr, channel->port)){
            erroroccours = 1;
            break;
        }
        
        /* enable write */
        if(0!=aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_WRITABLE,_write_callback,(void*)channel)){
            erroroccours = 1;
            break;
        }
        channel->flg |= _FLG_SEND_ENABLED;
    }while(0);
    
    if(!erroroccours){
        /* assign handler */
        channel->handler = handler;
        /* assign timeouts */
        channel->timeouts = timeouts;
        /* assign pUsr */
        channel->pUsr = pUsr;
        channel->lastinteraction=time(NULL);
    }
    
    return erroroccours?0:1;
}

bool channel_send(channel_t *channel, char *data, int len)
{    
    /* append data to send data queue */
    dataqueue_insert_data(&channel->outDataqueue, data, len);
    
    /* check write enabled, if not enabled, enable it */
    if(channel->flg & _FLG_SEND_ENABLED){
        return true;
    }
    
    /* add send event */
    if(0==aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_WRITABLE,_write_callback,(void*)channel)){
        channel->flg |= _FLG_SEND_ENABLED;
        return true;
    }
    
	return false;
}

static void _check_timeouts(channel_t *head)
{
    channel_t *tail = head->prev;
    channel_t *node = head;
    channel_t *temp = NULL;
    unsigned int now = time(NULL);
    
    /* cycle check until (tail-1) */
    do {
        temp = node->next; /* remember next */
        
        /* check connecting timeout */
        if((-1)==node->timeouts&&!node->connected&&node->lastinteraction+_MAX_CONNECTING_SECS<now){
            /* close channel */
            channel_close(node, (-1));
        }
        else if((-1)!=node->timeouts&&node->lastinteraction+node->timeouts<now){
            /* close channel */
            channel_close(node, (-1));
        }
        node = temp;
    }while(node!=tail);
}

void channel_crond(int loops)
{    
    if(0==(loops % _CHANNEL_LOOPS_NUM)){
        if(g_libnet.channelHead){
            _check_timeouts(g_libnet.channelHead);
        }        
    }
}
