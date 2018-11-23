#ifndef PACK_H
#define PACK_H


unsigned short make_msg_packet(char **cbBuffer, unsigned short wMainCmdID, unsigned short wSubCmdID, char* pData, unsigned short wDataSize);


#endif