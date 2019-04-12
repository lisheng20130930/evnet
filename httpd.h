/*
 * httpd.h 
 *
 * The http server API for http(s) app server
 *
 * Copyright (C) Listen.Li, 2018
 *
 */
#ifndef HTTP_D_H
#define HTTP_D_H


#include "buffer.h"
#include "listlib.h"
#include "httparser.h"


enum{estatusnone=0,estatnodeeady,estatushandle,estatusdone};


typedef struct _node_s node_t;
typedef void (*pfn_node_send)(node_t *node, char *data, int size, int isLastPacket);

typedef struct header_s{
	list_head hd;
	char *fieldname;
	char *fieldvalue;
}header_t;

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
	list_head headers;
	union{
		struct {
			char pszPathName[1024];
			unsigned int headsize;
			unsigned int total;			
		}d;
	}u;
};


enum{
	ENODEHANDLE = 0, //бф|идик
	ENODECLEAR = 1, //??идик
	ENODECONTINUE = 2 //?имD?
};


typedef void (*node_handle_t)(node_t *node, int evt, int p1, int p2);


char* getFiledvalue(node_t *node, char *fieldname);
char* getRemoteAddr(node_t *node);

void* httpd_start(node_handle_t handler, unsigned short port, int maxCon, int timeout, int secrity, char *cert);
void httpd_stop(void *httpd);



#endif