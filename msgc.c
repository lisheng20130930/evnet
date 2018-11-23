#include "libos.h"
#include "evfunclib.h"
#include "msg_pack.h"
#include "msgc.h"



static bool msgc_continte(void *pUsr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize)
{
	msgc_t *msgC = (msgc_t*)pUsr;
	return msgC->handler(msgC,wMainCmdID,wSubCmdID,pData,wDataSize);
}


static int _channel_callback(void *pUsr, void *msg, unsigned int size)
{
	msgChannel_t *msgChannel = (msgChannel_t*)msg;
	msgc_t *msgC = (msgc_t*)pUsr;
	
    switch(msgChannel->identify){
    case _EVDATA:
		{
			DBGPRINT(EERROR,("[MSGC] __EVDATA-SIZE=%d\r\n",dataqueue_datasize(msgChannel->u.dataqueue)));
			if(!msgparser_parser(&msgC->parser, msgChannel->u.dataqueue, msgc_continte, msgC)){
				evnet_closechannel(msgChannel->channel,0);
			}
		}
        break;
    case _EVCLOSED:
		if(msgC->pfnclose){
			msgC->pfnclose(msgC);
		}
		DBGPRINT(EERROR,("[MSGC] EVclosed.\r\n"));
        break;
    default:
        break;
    }
    return 0;
}


bool msgc_open(msgc_t *msgC, msgc_handle_t handler, on_close_t pfnclose, void *usrdata, char* ipStr, unsigned short port, int timeout)
{		
	msgC->handler = handler;
	msgC->pfnclose = pfnclose;
	msgC->usrdata = usrdata;
	msgC->timeout = timeout;
	msgC->c = evnet_createchannel(ipStr,port,0);
	if(!msgC->c){
		return false;
	}
	evnet_channelbind(msgC->c, _channel_callback, timeout, (void*)msgC);
	return true;
}

bool msgc_send(msgc_t *msgc, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize)
{
	msgparser_t *parser = &msgc->parser;
	if(NULL==msgc->c){
		return false;
	}    
	char *cbDataBuffer = NULL;
    unsigned short wSendSize = make_msg_packet(&cbDataBuffer,wMainCmdID,wSubCmdID,pData,wDataSize);
	parser->sendPacketCount++;
	return evnet_channelsend(msgc->c, (char*)cbDataBuffer, wSendSize);
}

void msgc_close(msgc_t *msgc, int errorcode)
{
	evnet_closechannel(msgc->c,errorcode);
}