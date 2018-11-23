/*
 * httpd.h 
 *
 * The httpD server API for http(s) app server
 *
 * Copyright (C) Listen.Li, 2018
 *
 */
#ifndef HTTP_D_H
#define HTTP_D_H


#include "buffer.h"
#include "httparser.h"


enum{estatusnone=0,estatnodeeady,estatushandle,estatusdone};


typedef struct _node_s node_t;
typedef void (*pfn_node_send)(node_t *node, char *data, int size, int isLastPacket);

struct _node_s{
	http_parser parser;	
	char *URL;
	unsigned char method;
	int status;
	void *usr;
	buffer_t buffer;
	pfn_node_send pfnSend;
	unsigned int sendsize;
	unsigned int sentsize;
	union{
		struct {
			char pszPathName[1024];
			unsigned int headsize;
			unsigned int total;			
		}d;
	}u;
};


enum{
	ENODECONTINUE = 0,
	ENODECLEAR = 1,
	ENODESENT = 2
};


typedef void (*node_handle_t)(node_t *node, int evt, int p1, int p2);



void* httpd_start(node_handle_t handler, unsigned short port, int maxCon, int timeout, int secrity, char *cert);
void httpd_stop(void *httpd);



#endif