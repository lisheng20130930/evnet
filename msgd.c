#include "libos.h"
#include "evfunclib.h"
#include "msg_pack.h"
#include "msgd.h"

typedef struct _msgd_s{
	usr_handle_t handler;
	pfnUsrClose pfnClose;
	int timeout;
	int maxCon;
	void *acceptor;
	int curCon;
}msgd_t;



static bool msgd_continte(void *pUsr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize)
{
	usrBase_t *usr = (usrBase_t*)pUsr;
	msgd_t *msgD = (msgd_t*)usr->msgd;	
	return msgD->handler(usr,wMainCmdID,wSubCmdID,pData,wDataSize);
}


static int _channel_callback(void *pUsr, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
	usrBase_t *usr = (usrBase_t*)pUsr;
	msgd_t *msgD = (msgd_t*)usr->msgd;
	
    switch(msgChannel->identify){
    case _EVDATA:
		DBGPRINT(EERROR,("[MSGD] dwUsrID = %u, __EVDATA-SIZE=%d\r\n", usr->gid, dataqueue_datasize(msgChannel->u.dataqueue)));
		if(!msgparser_parser(&usr->parser, msgChannel->u.dataqueue, msgd_continte, usr)){
			evnet_closechannel(msgChannel->channel,0);
		}	
        break;
    case _EVCLOSED:
		if(msgD->pfnClose){
			msgD->pfnClose(usr);
		}
		msgD->curCon--;
		DBGPRINT(EERROR,("[MSGD] EVclosed (usr-gid=%u)...curCon: %d\r\n",usr->gid,msgD->curCon));
		free(usr);
        break;
    default:
        break;
    }
    return 0;
}


static bool usr_send(usrBase_t *usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize)
{
	msgparser_t *parser = &usr->parser;
	if(NULL==usr->c){
		return false;
	}    
	char *cbDataBuffer = NULL;
    unsigned short wSendSize = make_msg_packet(&cbDataBuffer,wMainCmdID,wSubCmdID,pData,wDataSize);
	parser->sendPacketCount++;
	return evnet_channelsend(usr->c, (char*)cbDataBuffer, wSendSize);
}


static void usr_close(usrBase_t *usr, int errorCode)
{
	evnet_closechannel(usr->c,errorCode);
}

static int _acceptor_callback(void *pUser, void *msg, unsigned int size)
{
    msgAcceptor_t *msgAcceptor = (msgAcceptor_t*)msg;
	msgd_t *msgD = (msgd_t*)pUser;
	static int g_ID = 0;
	usrBase_t *usr = NULL;
	
	if(msgD->curCon>=msgD->maxCon){
		DBGPRINT(EERROR,("[MSGD] Too much Connect curCon=%d\r\n",msgD->curCon));
		evnet_closechannel(msgAcceptor->u.channel,0);
		return 0;
	}
	
	usr = (usrBase_t*)malloc(sizeof(usrBase_t));
	memset(usr,0x00,sizeof(usrBase_t));
	usr->c = msgAcceptor->u.channel;
	usr->send = usr_send;
	usr->close = usr_close;
	usr->gid = g_ID++;
	usr->msgd = msgD;
	
    evnet_channelbind(msgAcceptor->u.channel,_channel_callback,msgD->timeout,(void*)usr);
    
	msgD->curCon++;
	DBGPRINT(EDEBUG,("[MSGD] usrAccepted..curCon=%d\r\n",msgD->curCon));
    return 0;
}

void* msgd_start(usr_handle_t handler, pfnUsrClose onClose, unsigned short port, int maxCon, int timeout)
{
	msgd_t *msgD = (msgd_t*)malloc(sizeof(msgd_t));
	memset(msgD,0x00,sizeof(msgd_t));
	msgD->handler = handler;
	msgD->pfnClose = onClose;
	msgD->maxCon = maxCon;
	msgD->timeout = timeout;
	msgD->acceptor = evnet_createacceptor(port, 0, NULL, _acceptor_callback, msgD);
    if(!msgD->acceptor){
		free(msgD);
		return NULL;
    }
	evnet_acceptorstart(msgD->acceptor);
	return msgD;
}

void msgd_stop(void *msgd)
{
	msgd_t *msgD = (msgd_t*)msgd;
	evnet_acceptorstop(msgD->acceptor);
	evnet_destroyacceptor(msgD->acceptor);
	free(msgD);
}