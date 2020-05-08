#include "libos.h"
#include "evfunclib.h"
#include "harbor.h"



static int g_msgId = 0;


static bool harbor_msgHandler(void* harbor, int msgType, harborMsg_t *msg)
{
    printf("harbor_msgHandler===> MsgType = %d   ", msgType);
    if(msgType==EMSGTYPE_ERROR){
        printf("harbor Error ...errorCode=%d, msgId=%d\r\n",msg->errorcode,msg->msgId);
    }else{
        printf("harbor recv message ...msgId=%d, response=%s\r\n",msg->msgId,msg->message);
    }

    return true;
}

int main(int argc, char **argv)
{
    evnet_init(3000);

    void *harbor = harbor_start("http://47.110.157.52:8000/pushService",harbor_msgHandler);
    
    unsigned int g_loop = 1;
    
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));
    harbor_send(harbor, ++g_msgId, "{\"cmd\": 5000}",strlen("{\"cmd\": 5000}"));

    while(1){        
        evnet_loop(g_loop);
        g_loop++;
    }
    evnet_uint();
    
    return 0;
}