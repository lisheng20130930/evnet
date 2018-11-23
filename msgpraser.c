#include "libos.h"
#include "msgparser.h"


static int _command(msgparser_t *coder, char **ppHead, dataqueue_t *in_dataqueue, unsigned short *pwPacktSize)
{
	static unsigned char g_cbRecvDataBuffer[SOCKET_PACKET+sizeof(CMD_Head)];
    bool erroroccours = false;
    int buff_len = 0;
    char *buff = NULL;
    int size = 0;
	
    while(1){
        if(!coder->isHeadGotten){
            /* check head size */
            if(dataqueue_datasize(in_dataqueue)<sizeof(CMD_Head)){
                break;
            }
            coder->pos = 0;  // RESET
            /* get head first */
            while(coder->pos < sizeof(CMD_Head)) {
                dataqueue_next_data(in_dataqueue, &buff, &buff_len);
                /* get head needed len */
                size = __min(buff_len, (int)sizeof(CMD_Head)-coder->pos);
                /* copy data */
                memcpy(g_cbRecvDataBuffer+coder->pos, buff, size);
                coder->pos += size;
                dataqueue_distill_data(in_dataqueue, size);
            }
			
            /* copy stCmdHead */
            memcpy(&coder->stCmdHead, g_cbRecvDataBuffer, sizeof(CMD_Head));
			
            /* check version */
            if(coder->stCmdHead.CmdInfo.cbVersion!=SOCKET_VER){    
				DBGPRINT(EERROR, ("[CCODER] Bad Socket Ver(%d)\r\n",coder->stCmdHead.CmdInfo.cbVersion));
                erroroccours = true;
                break;
            }
            
            /* asign flg */
            coder->isHeadGotten = true;
        }
        else {
            /* package is too large */
            if(coder->stCmdHead.CmdInfo.wPacketSize>SOCKET_PACKET){
				DBGPRINT(EERROR, ("[CCODER] Bad wPacketSize(%d)\r\n",coder->stCmdHead.CmdInfo.wPacketSize));
                erroroccours = true;
                break;
            }
			
            /* package is not completed */
            if(dataqueue_datasize(in_dataqueue)+sizeof(CMD_Head)<coder->stCmdHead.CmdInfo.wPacketSize){
                break;
            }
			
			/* copy head back */
			memcpy(g_cbRecvDataBuffer, &coder->stCmdHead, sizeof(CMD_Head));
            
            /* copy data */
            while(coder->pos < coder->stCmdHead.CmdInfo.wPacketSize) {
                dataqueue_next_data(in_dataqueue, &buff, &buff_len);
                size = __min(buff_len, coder->stCmdHead.CmdInfo.wPacketSize-coder->pos);
                /* copy data */
                memcpy(g_cbRecvDataBuffer+coder->pos, buff, size);
                coder->pos += size;
                dataqueue_distill_data(in_dataqueue, size);
            }
            
            /* assign size */
            *pwPacktSize = coder->stCmdHead.CmdInfo.wPacketSize;
            *ppHead = (char*)g_cbRecvDataBuffer;
			
            /* assin flg */
            coder->isHeadGotten = false;
            break;
        }
    };
	
    return erroroccours?false:true;
}

bool msgparser_parser(msgparser_t *coder, dataqueue_t *dataqueue, msg_continte_t pfn, void *pUsr)
{
	bool bRet = true;	
	
	while(0<dataqueue_datasize(dataqueue)){
		char *cbDataBuffer = NULL;
		unsigned short wPacketSize = 0;
		
		if(!_command(coder, (char**)&cbDataBuffer, dataqueue, &wPacketSize)){
			DBGPRINT(EERROR, ("[CCODER] obtain client command failure\r\n"));
			bRet = false;
			break;
		}
		
		if (NULL == cbDataBuffer) {
			break;
		}
				
		if (wPacketSize < sizeof(CMD_Head)) {
			bRet = false;
			DBGPRINT(EERROR, ("[CCODER] badPacket\r\n"));
			break;
		}
		
		unsigned short wDataSize = wPacketSize - sizeof(CMD_Head);
		void *pDataBuffer = cbDataBuffer + sizeof(CMD_Head);
		CMD_Command Command = ((CMD_Head*)cbDataBuffer)->CommandInfo;
		
		coder->recvPacketCount++;		
		if(!pfn(pUsr, Command.wMainCmdID, Command.wSubCmdID, (char*)pDataBuffer, wDataSize)){
			bRet = false;
			break;
		}
	}

	return bRet;
}