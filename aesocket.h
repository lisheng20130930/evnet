#ifndef _AESOCKET_H_
#define _AESOCKET_H_


#define MAX_CNN   (80*1024)

#ifdef WIN32
/* we must first define FD_SETSIZE before winsock2.h */
#define FD_SETSIZE  MAX_CNN
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
#endif

#define _wouldblock (-2)

typedef int fd_t;

#define _INVALIDFD  (-1)

void aesocketinit();
void aesocketuint();
fd_t aesoccreate(int acceptor);
int aesoclisten(fd_t s, unsigned short port);
int aesocaccept(fd_t s, fd_t *sa, unsigned int *ip, unsigned short *port);
unsigned int aehostbyname(char *name);
void aesocclose(fd_t s);
int aesocconnect(fd_t s, unsigned int ip, unsigned short port);
int aesocwrite(fd_t s, char *pszBuff, int iSize);
int aesocread(fd_t s, char *pszBuff, int iSize);


#endif
