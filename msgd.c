#include "libos.h"
#include "evfunclib.h"
#include "msg_pack.h"
#include "msgd.h"


typedef struct _msgd_s{
	conn_handle_t handler;
	conn_close_t pfnClose;
	int timeout;
	int maxCon;
	void *acceptor;
	int curCon;
}msgd_t;



static bool msgd_continte(void *pUsr, unsigned short wMainCmdID, unsigned short wSubCmdID, char *pData, unsigned short wDataSize)
{
	conn_t *conn = (conn_t*)pUsr;
	msgd_t *msgD = (msgd_t*)conn->msgd;	
	return msgD->handler(conn,wMainCmdID,wSubCmdID,pData,wDataSize);
}


static int _channel_callback(void *pUsr, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
	conn_t *conn = (conn_t*)pUsr;
	msgd_t *msgD = (msgd_t*)conn->msgd;
	
    switch(msgChannel->identify){
    case _EVDATA:
		DBGPRINT(EERROR,("[MSGD] dwUsrID = %u, __EVDATA-SIZE=%d\r\n", conn->gid, dataqueue_datasize(msgChannel->u.dataqueue)));
		if(!msgparser_parser(&conn->parser, msgChannel->u.dataqueue, msgd_continte, conn)){
			evnet_closechannel(msgChannel->channel,0);
		}	
        break;
    case _EVCLOSED:
		if(msgD->pfnClose){
			msgD->pfnClose(conn);
		}
		msgD->curCon--;
		DBGPRINT(EERROR,("[MSGD] EVclosed (conn-gid=%u)...curCon: %d\r\n",conn->gid,msgD->curCon));
		free(conn);
        break;
    default:
        break;
    }
    return 0;
}


static bool conn_send(conn_t *conn, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize)
{
	msgparser_t *parser = &conn->parser;
	if(NULL==conn->c){
		return false;
	}    
	char *cbDataBuffer = NULL;
    unsigned short wSendSize = make_msg_packet(&cbDataBuffer,wMainCmdID,wSubCmdID,pData,wDataSize);
	parser->sendPacketCount++;
	return evnet_channelsend(conn->c, (char*)cbDataBuffer, wSendSize);
}


static void conn_close(conn_t *conn, int errorCode)
{
	evnet_closechannel(conn->c,errorCode);
}

static int _acceptor_callback(void *pUser, void *msg, unsigned int size)
{
    msgAcceptor_t *msgAcceptor = (msgAcceptor_t*)msg;
	msgd_t *msgD = (msgd_t*)pUser;
	static int g_ID = 0;
	conn_t *conn = NULL;
	
	if(msgD->curCon>=msgD->maxCon){
		DBGPRINT(EERROR,("[MSGD] Too much Connect curCon=%d\r\n",msgD->curCon));
		evnet_closechannel(msgAcceptor->u.channel,0);
		return 0;
	}
	
	conn = (conn_t*)malloc(sizeof(conn_t));
	memset(conn,0x00,sizeof(conn_t));
	conn->c = msgAcceptor->u.channel;
	conn->send = conn_send;
	conn->close = conn_close;
	conn->gid = g_ID++;
	conn->msgd = msgD;
	
    evnet_channelbind(msgAcceptor->u.channel,_channel_callback,msgD->timeout,(void*)conn);
    
	msgD->curCon++;
	DBGPRINT(EDEBUG,("[MSGD] connAccepted..curCon=%d\r\n",msgD->curCon));
    return 0;
}

void* msgd_start(conn_handle_t handler, conn_close_t _close, unsigned short port, int maxCon, int timeout)
{
	msgd_t *msgD = (msgd_t*)malloc(sizeof(msgd_t));
	memset(msgD,0x00,sizeof(msgd_t));
	msgD->handler = handler;
	msgD->pfnClose = _close;
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