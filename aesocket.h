#ifndef _AESOCKET_H_
#define _AESOCKET_H_

#ifdef WIN32
/* we must first define FD_SETSIZE before winsock2.h */
#define FD_SETSIZE    (4096)
#include "stdlib.h"
#include "windows.h"
#include "winsock2.h"
#include "io.h"
#else
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#endif

#define _wouldblock (-2)

typedef int fd_t;

#define _INVALIDFD  (-1)


static __inline int isIPv6Addr(char *ipStr)
{
    if(strchr((const char*)ipStr,':')){
        return 1;
    }
    return 0;
}


void aesocketinit();
void aesocketuint();
fd_t aesoccreate(int family, int acceptor);
int aesoclisten(fd_t s, unsigned short port);
int aesocaccept(fd_t s, fd_t *sa, char ipStr[], int len, unsigned short *port);
void aehostbyname(char *name, char ipStr[], int len); //128
void aesocclose(fd_t s);
int aesocconnect(fd_t s, char *ipStr, unsigned short port);
int aesocwrite(fd_t s, char *pszBuff, int iSize);
int aesocread(fd_t s, char *pszBuff, int iSize);



#endif
