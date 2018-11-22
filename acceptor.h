#ifndef ACCEPTOR_H
#define ACCEPTOR_H


typedef struct _acceptor_s
{
    pfn_msg_handler handler;
    fd_t fd;
    void *pUsr;
	#ifdef SSL_SUPPORT
	int secrity; //IsSecrity
	void *ctx; //SSLCTX
	#endif
}acceptor_t;


acceptor_t* acceptor_create(int family, unsigned short port, int secrity, char *cert, pfn_msg_handler handler, void *pUsr);
void acceptor_destroy(acceptor_t *acceptor);
int acceptor_start(acceptor_t *acceptor);
void acceptor_stop(acceptor_t *acceptor);


#endif
