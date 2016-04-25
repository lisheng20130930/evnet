#ifndef CHANNEL_H
#define CHANNEL_H

typedef struct channel_s
{
    struct channel_s *next;
    struct channel_s *prev;
    pfn_msg_handler handler;  /* handler */
    void *pUsr;    
    dataqueue_t inDataqueue; /* in data queue */
    dataqueue_t outDataqueue; /* out data queue */
    unsigned int lastinteraction; /* time of the last interaction, used for timeout */
    unsigned int timeouts;
    fd_t            fd;
    unsigned int    ip;
    unsigned short  port;
    unsigned int    flg;
    int upstream;
    int connected;
}channel_t;


channel_t* channel_create(fd_t fd, unsigned int ip, unsigned short port, int upstream);
void channel_close(channel_t *channel, int errcode);
int channel_bind(channel_t *channel, pfn_msg_handler handler, unsigned int timeouts, void *pUsr);
bool channel_send(channel_t *channel, char *data, int size);
void channel_crond(int loops);


static __inline
unsigned int channel_ip(channel_t *channel)
{
    return channel->ip;
}

static __inline
void* channel_user(channel_t *channel)
{
    return channel->pUsr;
}

static __inline
channel_t* upstream_channel(unsigned int ip, unsigned short port)
{
    fd_t fd = aesoccreate(0);
    if(_INVALIDFD==fd){
        return NULL;
    }
    return channel_create(fd,ip,port,1);
}

#endif
