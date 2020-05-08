#ifndef HARBOR_H
#define HARBOR_H


enum{
    EMSGTYPE_REMOTE = 0, //remote's message
    EMSGTYPE_ERROR  //ERROR
};


typedef struct harborMsg_s{
    int errorcode;
    int msgId;
    char *message;
    int len;  
}harborMsg_t;


typedef bool (*msg_callback)(void* harbor, int msgType, harborMsg_t *msg);


void harbor_send(void* harbor, int msgId, char *msg, int len);

void* harbor_start(char *szURL, msg_callback callback);
void harbor_stop(void* harbor);



#endif