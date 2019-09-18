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
#include "muparser.h"


enum{estatusnone=0,estatnodeeady,estatushandle,estatusdone};


typedef struct node_t node_t;
typedef void (*pfn_node_send)(node_t *node, char *data, int size, int isLastPacket);

typedef struct header_s{
	struct list_head hd;
	char *fieldname;
	char *fieldvalue;
}header_t;


/* 
* usr should define <struct node_t>
* such as:
* struct node_t{
*    struct node_s base;
*    xxxx  // your extention member1
*    yyyyy // your extention member2
* };
* 
* or you can copy struct _usr_s members to it and must be sure the same structure
* struct node_t{
*    struct node_s members HERE
*    //your part now
*    xxxx  // your extention member1
*    yyyyy // your extention member2
* };
* 
* Give the sizeof(usr_t) as param to msgd_start
*
*/
struct node_s{
	http_parser parser;	
	char *URL;
	unsigned char method;
	int status;
	void *usr;	
	pfn_node_send pfnSend;
	unsigned int sendsize;
	unsigned int sentsize;
	struct list_head headers;
	int IsFileUpload;
	union{
		buffer_t buffer;
		struct{
			multipart_parser *mp;
			struct list_head mhdr;
			FILE *pFile;
			bool isComplete; //upload success
			char filename[256];
		}upload;
	}req;

	union{
		struct {
			char pszPathName[1024];
			unsigned int headsize;
			unsigned int total;			
		}d;
	}u;
};


enum{
	ENODEHANDLE = 0,
	ENODECLEAR = 1,
	ENODECONTINUE = 2
};


typedef void (*node_handle_t)(node_t *node, int evt, int p1, int p2);


char* getFiledvalue(node_t *node, char *fieldname);
char* getRemoteAddr(node_t *node);

void* httpd_start(node_handle_t handler, int nodesize, unsigned short port, int timeout, int secrity, char *cert);
void httpd_stop(void *httpd);



#endif