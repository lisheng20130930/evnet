#ifndef MSG_D_H
#define MSG_D_H


#include "msgparser.h"


typedef struct usrBase_s{
	bool (*send)(struct usrBase_s *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);
	void (*close)(struct usrBase_s *usr, int errorCode);
	unsigned char cbUsrStatus;
	msgparser_t parser;
	unsigned int gid; //id OF IT
	void *c;
	void *usrdata; //usrdata
	void *msgd;
}usrBase_t;


typedef bool (*usr_handle_t)(usrBase_t *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize);
typedef void (*pfnUsrClose)(usrBase_t *usr);


void* msgd_start(usr_handle_t handler, pfnUsrClose onClose, unsigned short port, int maxCon, int timeout);
void msgd_stop(void *msgd);



#endif