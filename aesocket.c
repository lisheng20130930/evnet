#include "aesocket.h"

#ifdef WIN32
#define EV_FD_TO_WIN32_HANDLE(fd) _get_osfhandle(fd)
#define EV_WIN32_HANDLE_TO_FD(handle) _open_osfhandle(handle, 0)

static int aesocnoblock(SOCKET soc)
{
    unsigned long argp = 1;    
    if(0 != ioctlsocket(soc, FIONBIO, &argp)){
        closesocket(soc);
        return 0;
    }    
    return 1;
}

fd_t aesoccreate(int acceptor)
{
    SOCKET soc = 0;
    int on = 1;
    
    soc = socket(AF_INET,SOCK_STREAM,0);
    if(INVALID_SOCKET == soc){
        return _INVALIDFD;
    }
    if(acceptor&&(setsockopt(soc,SOL_SOCKET,SO_REUSEADDR,(const char*)&on,sizeof(on))==-1)){
        closesocket(soc);
        return _INVALIDFD;
    }    
    if(!aesocnoblock(soc)){
        closesocket(soc);
        return _INVALIDFD;
	}

    return EV_WIN32_HANDLE_TO_FD(soc);
}

int aesoclisten(fd_t s, unsigned short usPort)
{
    SOCKET socket = EV_FD_TO_WIN32_HANDLE(s);
    SOCKADDR_IN addrSrv;
    
    addrSrv.sin_addr.S_un.S_addr = INADDR_ANY;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(usPort);       
    if(SOCKET_ERROR==bind(socket, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR))){
        return (-1);
    }
    if(SOCKET_ERROR==listen(socket, SOMAXCONN)){
        return (-1);
    }    
    return 0;
}

int aesocaccept(fd_t s, fd_t *sa, unsigned int *ip, unsigned short *port)
{
    SOCKET socket = EV_FD_TO_WIN32_HANDLE(s);
    SOCKET sockConn = INVALID_SOCKET;
    SOCKADDR_IN  addrIn;
    int len = sizeof(SOCKADDR);    
    
    sockConn = accept(socket, (SOCKADDR*)&addrIn, &len);    
    if(INVALID_SOCKET == sockConn) {
        return _wouldblock;
    }    
    if(SOCKET_ERROR == sockConn){     
        return (WSAGetLastError()==WSAEWOULDBLOCK) ? _wouldblock : (-1);
    }    
    if(!aesocnoblock(sockConn)){
        closesocket(sockConn);
        return (-1);
    }
    if(ip){
        *ip = addrIn.sin_addr.s_addr;
    }
    if(port){
        *port = ntohs(addrIn.sin_port);
    }    
    *sa = EV_WIN32_HANDLE_TO_FD(sockConn);    
    return 0;
}   


int aesocconnect(fd_t iSocket, unsigned int ip, unsigned short port)
{
    SOCKET socket = EV_FD_TO_WIN32_HANDLE(iSocket);
    struct sockaddr_in addr;
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(ip);    
    if(SOCKET_ERROR==connect(socket,(const struct sockaddr*)&addr,sizeof(addr))){
        if(WSAEWOULDBLOCK!=WSAGetLastError()){
            return (-1);
        }
    }
    return 0;
}

void aesocclose(fd_t iSoc)
{
    _close(iSoc);
}

int aesocwrite(fd_t iSoc, char *pszBuff, int iLen)
{
    SOCKET socket = EV_FD_TO_WIN32_HANDLE(iSoc);
    int ret = 0;

    if(0 >= iLen){
        return 0;
    }  
    ret = send(socket, pszBuff, iLen, 0);
    if(SOCKET_ERROR == ret){
        if(WSAEWOULDBLOCK!=WSAGetLastError()){
            return (-1);
        }
        return 0;
    }

    return (0>=ret)?0:ret;
}

int aesocread(fd_t s, char *pszBuff, int iLen)
{
    SOCKET socket = EV_FD_TO_WIN32_HANDLE(s);
    int ret = 0;

    if(0 >= iLen){
        return 0;
    }
  
    ret = recv(socket, pszBuff, iLen, 0);        
    if(SOCKET_ERROR == ret){
        if(WSAEWOULDBLOCK!=WSAGetLastError()){
            return (-1);
        }        
        return 0;
    }    
    return (0>=ret)?0:ret;
}

void aesocketinit()
{
    unsigned short wVersionRequested=MAKEWORD(1,1);
    WSADATA wsaData={0};
    
    WSAStartup(wVersionRequested, &wsaData);
}

void aesocketuint()
{
    WSACleanup();
}

unsigned int aehostbyname(char *name)
{
    struct hostent  *pHost = NULL;
    int j = 0;
    char *pszTemp = NULL;
	unsigned int ip = 0;
       
    if(!(pHost=gethostbyname(name))){
        return 0;  
    }
    pszTemp = (char*)&ip;
    for(j=0;j<1&&NULL!=*(pHost->h_addr_list);pHost->h_addr_list++,j++){
        memcpy(pszTemp,*(pHost->h_addr_list),pHost->h_length);
        break;
    }
    return ip;
}
#else
int aesocnoblock(fd_t soc)
{
	int flags = 0;

    flags = fcntl(soc, F_GETFL, 0);
    if(flags == -1){
		return 0;
	}    
	if(fcntl(soc,F_SETFL,flags|O_NONBLOCK )<0){
		return 0;
	}
    return 1;
}

fd_t aesoccreate(int acceptor)
{
    fd_t soc = 0;
	int on = 1;
    
    soc = socket(AF_INET,SOCK_STREAM,0);
    if((-1)==soc){
        return _INVALIDFD;
    }
    if(acceptor&&(setsockopt(soc,SOL_SOCKET,SO_REUSEADDR,(const char*)&on,sizeof(on))==-1)){
        close(soc);
        return _INVALIDFD;
    }
    if(!aesocnoblock(soc)){
		close(soc);
		return _INVALIDFD;
	}
    return soc;
}

extern int aesocconnect(fd_t iSocket, unsigned int ip, unsigned short port)
{
    struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(ip);
	if((-1)==connect(iSocket,(const struct sockaddr*)&addr,sizeof(addr))){
		if(errno!=EINPROGRESS){
			return (-1);
		}
	}
    return 0;
}

int aesoclisten(fd_t s, unsigned short port)
{
    struct sockaddr_in sa_in;
    
    sa_in.sin_addr.s_addr = INADDR_ANY;
    sa_in.sin_family = AF_INET;
    sa_in.sin_port = htons(port);
    
    if(0>bind(s,(struct sockaddr*)&sa_in, sizeof(struct sockaddr_in))){        
        return (-1);
    }
    if(0>listen(s, 128)){
        return (-1);
    }    
    return 0;
}

int aesocaccept(fd_t s, fd_t *sa, unsigned int *ip, unsigned short *port)
{
    fd_t sockConn = _INVALIDFD;
    struct sockaddr addrIn;
    int len = sizeof(struct sockaddr);    

    sockConn = accept(s, &addrIn, &len);
    if((-1)==sockConn) {
        return _wouldblock;
    }
    if(0>sockConn) {    
        return (errno == EWOULDBLOCK)?_wouldblock:(-1);
    }    
    if(!aesocnoblock(sockConn)){        
        close(sockConn);
        return (-1);
    }
    if(ip){
        *ip = ((struct sockaddr_in*)&addrIn)->sin_addr.s_addr;
    }
    if(port){
        *port = ntohs(((struct sockaddr_in*)&addrIn)->sin_port);
    }    
    *sa = sockConn;    
    return 0;
}   


void aesocclose(fd_t iSoc)
{
    close(iSoc);
}

int aesocwrite(fd_t iSoc, char *pszBuff, int iLen)
{
    int ret = 0;

    if(0 >= iLen){
        return 0;
    }  
    ret = send(iSoc, pszBuff, iLen, 0);
    if(0 > ret){
        if(errno!=EWOULDBLOCK){
            return (-1);
        }        
        return 0;
    }
    return (0>=ret)?0:ret;
}

int aesocread(fd_t s, char *pszBuff, int iLen)
{
    int ret = 0;

    if (0 >= iLen) {
        return 0;
    }
  
    ret = recv(s, pszBuff, iLen, 0);        
    if(0>ret){
        if (errno!=EWOULDBLOCK&&errno!=EINPROGRESS&&errno!=EINTR&&errno!=EAGAIN){
            return (-1);
        }
        return 0;
    }
    
    return (0>=ret)?0:ret;
}

void aesocketinit()
{
}

void aesocketuint()
{
}

unsigned int aehostbyname(char *name)
{
	struct hostent *pHost = NULL;
	int j = 0;
	char *pszTemp = NULL;
	unsigned int uIP = 0;

	if(!(pHost=gethostbyname(name))){
		return 0;
	}
	pszTemp = (char*)&uIP;
	for(j=0;j<1&&NULL!=*(pHost->h_addr_list);pHost->h_addr_list++,j++)
	{
		memcpy(pszTemp,*(pHost->h_addr_list),pHost->h_length);
		break;
	}
	return uIP;
}
#endif
