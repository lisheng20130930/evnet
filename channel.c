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


channel_t* channel_create(fd_t fd, unsigned int ip, unsigned short port, int upstream)
{
    channel_t *channel = NULL;

    channel = (channel_t*)malloc(sizeof(channel_t));
    if(!channel){
        return NULL;
    }
    memset(channel, 0x00, sizeof(channel_t));
    channel->fd = fd;
    channel->ip = ip;
    channel->port = port;
    channel->upstream = upstream;

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

static void _read_callback(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask)
{
    channel_t *channel = (channel_t*)clientdata;
    pfn_msg_handler handler = channel->handler;
    void *pUsr = channel->pUsr;
    msgChannel_t msg = {0};
    int size = 0;
    
    /* recv data to data queue */
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

static void _write_callback(struct aeEventLoop *eventLoop, fd_t fd, void *pUsr, int mask)
{
    channel_t *channel = (channel_t*)pUsr;
    char *buff = NULL;
    int buff_len = 0;
    int size = 0;
    int isjusttry = 0;
    
    /* save just try and modify */
    isjusttry = channel->isjusttry;
    channel->isjusttry = 0;
    
    /* set connected to 1 */
    if(!channel->connected){
        channel->connected = 1;
        if(channel->upstream){
            /* enable read */
            if(0!=aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_READABLE,_read_callback,(void*)channel)){              
                channel_close(channel, (-1));
                return;
            }
            channel->flg |= _FLG_RECV_ENABLED;
        }
    }
    
    if(0 < dataqueue_datasize(&channel->outDataqueue)){
        /* next data */
        dataqueue_next_data(&channel->outDataqueue, &buff, &buff_len);
        size = aesocwrite(channel->fd, buff, buff_len);
        if(size < 0){            
            channel_close(channel, (-1));
            return;
        }
        
        if(0 == size){
            if(!isjusttry){
                channel_close(channel, (-1));
                return;
            }
        }
        
        channel->lastinteraction = time(NULL);
        dataqueue_distill_data(&channel->outDataqueue, size);
    }
    
    /* has data to send */
    if(0<dataqueue_datasize(&channel->outDataqueue)){
        return;
    }
    
    if(channel->flg & _FLG_SEND_ENABLED){
        aeDeleteFileEvent(g_libnet.evLoop, channel->fd, AE_WRITABLE);
        channel->flg &= ~_FLG_SEND_ENABLED;
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
        if (0!=aesocconnect(channel->fd, _HTONL(channel->ip), channel->port)){
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

void channel_send(channel_t *channel, char *data, int len)
{    
    /* append data to send data queue */
    dataqueue_insert_data(&channel->outDataqueue, data, len);
    
    /* check write enabled, if not enabled, enable it */
    if(channel->flg & _FLG_SEND_ENABLED){
        return;
    }
    
    /* add send event */
    if(0==aeCreateFileEvent(g_libnet.evLoop,channel->fd,AE_WRITABLE,_write_callback,(void*)channel)){
        channel->flg |= _FLG_SEND_ENABLED;
        if(channel->connected){
            channel->isjusttry = 1;
            _write_callback(g_libnet.evLoop,channel->fd,(void*)channel,AE_WRITABLE);
        }
        return;
    }
    
    /* close */
    channel_close(channel, (-1));
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
