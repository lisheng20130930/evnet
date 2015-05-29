#ifndef ACCEPTOR_H
#define ACCEPTOR_H


typedef struct _acceptor_s
{
    pfn_msg_handler handler;
    fd_t fd;
    void *pUsr;
}acceptor_t;


acceptor_t* acceptor_create(unsigned short port, pfn_msg_handler handler, void *pUsr);
void acceptor_destroy(acceptor_t *acceptor);
int acceptor_start(acceptor_t *acceptor);
void acceptor_stop(acceptor_t *acceptor);


#endif
