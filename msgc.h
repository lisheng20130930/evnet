#ifndef MSG_C_H
#define MSG_C_H


#include "msgparser.h"


typedef struct msgc_s msgc_t;

typedef bool (*msgc_handle_t)(msgc_t *msgc, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize);
typedef void (*on_close_t)(msgc_t *msgc);


struct msgc_s{
	msgc_handle_t handler;
	on_close_t pfnclose;
	void *usrdata;
	msgparser_t parser;
	void *c;
	int timeout;
};


bool msgc_open(msgc_t *msgC, msgc_handle_t handler, on_close_t pfnclose, void *usrdata, char* ipStr, unsigned short port, int timeout);
bool msgc_send(msgc_t *msgc, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);
void msgc_close(msgc_t *msgc, int errorcode);



#endif