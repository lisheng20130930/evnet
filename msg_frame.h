#ifndef MSG_CMD_FRAME_H
#define MSG_CMD_FRAME_H


#define SOCKET_VER						(0x66)						//netmodule version


#define SOCKET_BUFFER					(32*1024)
#define SOCKET_PACKET					(SOCKET_BUFFER-sizeof(CMD_Head)-2*sizeof(unsigned int))


typedef struct _CMD_Info{
	unsigned char					cbVersion;
	unsigned char					cbCheckCode;
	unsigned short					wPacketSize;
}CMD_Info;

typedef struct _CMD_Command{
	unsigned short					wMainCmdID;
	unsigned short					wSubCmdID;
}CMD_Command;

typedef struct _CMD_Head{
	CMD_Info						CmdInfo;
	CMD_Command						CommandInfo;
}CMD_Head;

typedef struct _CMD_Buffer{
	CMD_Head						Head;
	unsigned char					cbBuffer[SOCKET_PACKET];
}CMD_Buffer;


#define MDM_KN_COMMAND				0

#define SUB_S_DETECT_SOCKET			1
#define SUB_C_DETECT_SOCKET			4


typedef struct _SUB_KN_DetectSocket{
    unsigned int				dwSendTickCount;
    unsigned int				dwRecvTickCount;
}SUB_KN_DetectSocket;



#endif