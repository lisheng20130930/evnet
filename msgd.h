/*
 * msgd.h 
 *
 * The msg(tcp) server API for msg(s) app server
 *
 * Copyright (C) Listen.Li, 2018
 *
 */
#ifndef MSG_D_H
#define MSG_D_H


#include "msgparser.h"


typedef struct conn_s{
	bool (*send)(struct conn_s *conn, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);
	void (*close)(struct conn_s *conn, int errorCode);
	unsigned char cbUsrStatus;
	msgparser_t parser;
	unsigned int gid; //id OF IT
	void *c;
	void *usr; //usr
	void *msgd;
}conn_t;


typedef bool (*conn_handle_t)(conn_t *conn, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize);
typedef void (*conn_close_t)(conn_t *conn);


void* msgd_start(conn_handle_t handler, conn_close_t _close, unsigned short port, int maxCon, int timeout);
void msgd_stop(void *msgd);



#endif