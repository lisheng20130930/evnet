#ifndef MSG_CODER_H
#define MSG_CODER_H


#include "msg_frame.h"
#include "dataqueue.h"


typedef struct msgparser_s{
	unsigned int recvPacketCount;
    unsigned int sendPacketCount;
    CMD_Head  stCmdHead;
    int isHeadGotten;
	int pos;
}msgparser_t;


typedef bool (*msg_continte_t)(void *pUsr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize);


bool msgparser_parser(msgparser_t *coder, dataqueue_t *dataqueue, msg_continte_t pfn, void *pUsr);


#endif