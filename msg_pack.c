#include "libos.h"
#include "msg_frame.h"
#include "msg_pack.h"


unsigned short make_msg_packet(char **cbBuffer, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize)
{
	static unsigned char g_cbSendDataBuffer[SOCKET_PACKET+sizeof(CMD_Head)];
	CMD_Head *pHead = (CMD_Head*)g_cbSendDataBuffer;
    unsigned short wSendSize = 0;
    
    wSendSize = (unsigned short)(sizeof(CMD_Head)+wDataSize);
	
    pHead->CommandInfo.wMainCmdID = wMainCmdID;
    pHead->CommandInfo.wSubCmdID = wSubCmdID;
	pHead->CmdInfo.cbCheckCode = 0;
    pHead->CmdInfo.wPacketSize = wSendSize;
    pHead->CmdInfo.cbVersion = SOCKET_VER;
    
	if (pData && wDataSize>0){
        memcpy(pHead+1, pData, wDataSize);
    }

	*cbBuffer = (char*)g_cbSendDataBuffer;
	return wSendSize;
}