#include "libos.h"
#include "evfunclib.h"
#include "msg_pack.h"
#include "msgd.h"


typedef struct _msgd_s{
	usr_handle_t handler;
	usr_close_t pfnClose;
	int timeout;
	int maxCon;
	void *acceptor;
	int curCon;
	int usrsize;
}msgd_t;



static bool msgd_continte(void *pUsr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize)
{
	struct _usr_s *usr = (struct _usr_s*)pUsr;
	msgd_t *msgD = (msgd_t*)usr->msgd;	
	return msgD->handler((usr_t*)usr,wMainCmdID,wSubCmdID,pData,wDataSize);
}


static int _channel_callback(void *pUsr, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
	struct _usr_s *usr = (struct _usr_s*)pUsr;
	msgd_t *msgD = (msgd_t*)usr->msgd;
	
    switch(msgChannel->identify){
    case _EVDATA:
		DBGPRINT(EERROR,("[Trace@MSGD] dwUsrID = %u, __EVDATA-SIZE=%d\r\n", usr->gid, dataqueue_datasize(msgChannel->u.dataqueue)));
		if(!msgparser_parser(&usr->parser, msgChannel->u.dataqueue, msgd_continte, usr)){
			evnet_closechannel(msgChannel->channel,0);
		}	
        break;
    case _EVCLOSED:
		if(msgD->pfnClose){
			msgD->pfnClose((usr_t*)usr);
		}
		msgD->curCon--;
		DBGPRINT(EERROR,("[Trace@MSGD] EVclosed (usr-gid=%u)...curCon: %d\r\n",usr->gid,msgD->curCon));
		free(usr);
        break;
    default:
        break;
    }
    return 0;
}


static bool usr_send(usr_t *_usr, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize)
{
	struct _usr_s *usr = (struct _usr_s*)_usr;
	msgparser_t *parser = &usr->parser;
	if(NULL==usr->c){
		return false;
	}    
	char *cbDataBuffer = NULL;
    unsigned short wSendSize = make_msg_packet(&cbDataBuffer,wMainCmdID,wSubCmdID,pData,wDataSize);
	parser->sendPacketCount++;
	return evnet_channelsend(usr->c, (char*)cbDataBuffer, wSendSize);
}


static void usr_close(usr_t *_usr, int errorCode)
{
	struct _usr_s *usr = (struct _usr_s*)_usr;
	evnet_closechannel(usr->c,errorCode);
}

static int _acceptor_callback(void *pUser, void *msg, unsigned int size)
{
    msgAcceptor_t *msgAcceptor = (msgAcceptor_t*)msg;
	msgd_t *msgD = (msgd_t*)pUser;
	static int g_ID = 0;
	struct _usr_s *usr = NULL;
	
	if(msgD->curCon>=msgD->maxCon){
		DBGPRINT(EERROR,("[Trace@MSGD] Too much Connect curCon=%d\r\n",msgD->curCon));
		evnet_closechannel(msgAcceptor->u.channel,0);
		return 0;
	}
	
	usr = (struct _usr_s*)malloc(msgD->usrsize);
	memset(usr, 0x00, msgD->usrsize);
	usr->c = msgAcceptor->u.channel;
	usr->send = usr_send;
	usr->close = usr_close;
	usr->gid = g_ID++;
	usr->msgd = msgD;
	
    evnet_channelbind(msgAcceptor->u.channel,_channel_callback,msgD->timeout,(void*)usr);
    
	msgD->curCon++;
	DBGPRINT(EDEBUG,("[Trace@MSGD] usrAccepted..curCon=%d\r\n",msgD->curCon));
    return 0;
}

void* msgd_start(int usrsize, usr_handle_t handler, usr_close_t _close, unsigned short port, int maxCon, int timeout)
{
	msgd_t *msgD = (msgd_t*)malloc(sizeof(msgd_t));
	memset(msgD,0x00,sizeof(msgd_t));
	msgD->handler = handler;
	msgD->pfnClose = _close;
	msgD->maxCon = maxCon;
	msgD->timeout = timeout;
	msgD->usrsize = usrsize;
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