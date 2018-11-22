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
    char    ipStr[IPSTRSIZE];
    unsigned short  port;
    unsigned int    flg;
    int upstream;
    int connected;
	#ifdef SSL_SUPPORT
	int security;  //IF
	void *ctx; //SSL ctx
	void *ssl; //SSL
	#endif
}channel_t;


channel_t* channel_create(fd_t fd, char *ipStr, unsigned short port, int upstream, int security);
void channel_close(channel_t *channel, int errcode);
int channel_bind(channel_t *channel, pfn_msg_handler handler, unsigned int timeouts, void *pUsr);
bool channel_send(channel_t *channel, char *data, int size);
void channel_crond(int loops);

#ifdef SSL_SUPPORT
void Evnet_initSSL();
#endif

static __inline
char* channel_ip(channel_t *channel)
{
    return channel->ipStr;
}

static __inline
unsigned int channel_port(channel_t *channel)
{
    return channel->port;
}

static __inline
void* channel_user(channel_t *channel)
{
    return channel->pUsr;
}

static __inline
channel_t* upstream_channel(char *ipStr, unsigned short port, int security)
{
    fd_t fd = aesoccreate(isIPv6Addr(ipStr)?AF_INET6:AF_INET,0);
    if(_INVALIDFD==fd){
        return NULL;
    }
    return channel_create(fd,ipStr,port,1,security);
}

#endif
