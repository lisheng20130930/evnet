#include "aesocket.h"
#include "libos.h"
#include "event.h"
#include "evpipe.h"


void evpipe_dummy(struct aeEventLoop *eventLoop, fd_t fd, void *clientdata, int mask)
{
    char buffer[256];
    while(pipe_read(fd, buffer, 255)>0);
    DBGPRINT(EERROR,("[Trace@ThttpD] evpipe_dummy Leave\r\n"));
}

#ifdef WIN32
static int pipe(SOCKET fd[2])
{
    SOCKET tcp1=-1, tcp2=-1;
    sockaddr_in name;
    memset(&name, 0x00, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int namelen = sizeof(name);
        
    int tcp = socket(AF_INET, SOCK_STREAM, 0) ;
    if (tcp == -1) {
        goto clean;
    }
    if (bind(tcp, (sockaddr*)&name, namelen) == -1) {
        goto clean;
    }
    if (listen(tcp, 5) == -1) {
        goto clean;
    }
    if (getsockname(tcp, (sockaddr*)&name, &namelen) == -1) {
        goto clean;
    }
    tcp1 = socket(AF_INET, SOCK_STREAM, 0) ;
    if (tcp1 == -1) {
        goto clean;
    }
    if (-1 == connect(tcp1, (sockaddr*)&name, namelen)) {
        goto clean;
    }
    
    tcp2 = accept(tcp, (sockaddr*)&name, &namelen) ;
    if (tcp2 == -1) {
        goto clean;
    }
    if (closesocket(tcp) == -1) {
        goto clean;
    }
    fd[0] = tcp1;
    fd[1] = tcp2;
    return 0;
clean:
    if (tcp != -1) {
        closesocket( tcp) ;
    }
    if (tcp2 != -1) {
        closesocket( tcp2) ;
    }
    if (tcp1 != -1) {
        closesocket( tcp1) ;
    }
    return - 1;
}

static int socket_nonblocking(SOCKET soc)
{
    unsigned long argp = 1;    
    if(0 != ioctlsocket(soc, FIONBIO, &argp)){
        closesocket(soc);
        return -1;
    }    
    return 0;
}

int ev_pipe(int fd[2]){
    SOCKET pair[2] = {0};
    if (pipe(pair) == 0) {
        if (socket_nonblocking(pair[0]) < 0 ||
            socket_nonblocking(pair[1]) < 0) {
            closesocket(pair[0]);
            closesocket(pair[1]);            
            return -1;
        }
        fd[0] = _open_osfhandle(pair[0],0);
        fd[1] = _open_osfhandle(pair[1],0);
        return 0;
    }
    return -1;
}

int pipe_write(int fd, void *buffer, unsigned int size)
{
    return aesocwrite(fd,(char*)buffer,size);
}

int pipe_read(int fd, void *buffer, unsigned int size)
{
    return aesocread(fd,(char*)buffer,size);
}

#else

static int socket_nonblocking(int soc)
{
    int flags = 0;
    
    flags = fcntl(soc, F_GETFL, 0);
    if(flags == -1){
        return 0;
    }    
    if(fcntl(soc,F_SETFL,flags|O_NONBLOCK )<0){
        return 0;
    }
    #ifdef __IOS__
    int set = 1;
    setsockopt(soc,SOL_SOCKET,SO_NOSIGPIPE,(void*)&set,sizeof(int));
    #endif
    return 1;
}

int ev_pipe(int fd[2])
{   
    int pair[2] = {0};
    if (pipe(pair) == 0) {
        if (socket_nonblocking(pair[0]) < 0 ||
            socket_nonblocking(pair[1]) < 0) {
            close(pair[0]);
            close(pair[1]);
            return -1;
        }
        fd[0] = pair[0];
        fd[1] = pair[1];
        return 0;
    }
    return -1;
}


int pipe_write(int fd, void *buffer, unsigned int size)
{
    return write(fd,(char*)buffer,size);
}

int pipe_read(int fd, void *buffer, unsigned int size)
{
    return read(fd,(char*)buffer,size);
}

#endif