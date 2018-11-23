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


/* 
* usr should define <struct usr_t>
* such as:
* struct usr_t{
*    struct _user_s base;
*    xxxx  // your extention member1
*    yyyyy // your extention member2
* };
* 
* or you can copy struct _usr_s members to it and must be sure the same structure
* struct usr_t{
*    bool (*send)(usr_t *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);
*    void (*close)(usr_t *usr, int errorCode);
*    unsigned char cbUsrStatus;
*    msgparser_t parser;
*    unsigned int gid; //id OF IT
*    void *c;
*    void *msgd;
*    //your part now
*    xxxx  // your extention member1
*    yyyyy // your extention member2
* };
* 
* Give the sizeof(usr_t) as param to msgd_start
*
*/
typedef struct usr_t usr_t;
struct _usr_s{
	bool (*send)(usr_t *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);
	void (*close)(usr_t *usr, int errorCode);
	unsigned char cbUsrStatus;
	msgparser_t parser;
	unsigned int gid; //id OF IT
	void *c;
	void *msgd;
};


typedef bool (*usr_handle_t)(usr_t *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize);
typedef void (*usr_close_t)(usr_t *usr);


void* msgd_start(int usrsize, usr_handle_t handler, usr_close_t _close, unsigned short port, int maxCon, int timeout);
void msgd_stop(void *msgd);



#endif