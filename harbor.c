#include "libos.h"
#include "httpc.h"
#include "harbor.h"
#include "listlib.h"


#define HARBOR_TIMEOUT  (5)


typedef struct harbor_message_s{
    list_head head;
    char *msg;
    int len;
    int msgId;
}harbor_message_t;


typedef struct harbor_s{
    list_head sendList;
    httpc_t httpc;
    char *szURL;
    msg_callback callback;
    bool isBusy;
}harbor_t;


static void harbor_continue(harbor_t *harbor);


static void harobr_httpHandler(void *pUsr, coutputer_t *output, int errorcode)
{
    harbor_t *harbor = (harbor_t*)pUsr;
    
    harbor_message_t *message = list_entry(harbor->sendList.next,harbor_message_t,head);
    list_del(&message->head);
    
    harbor->isBusy = false;
    bool bRet = false;

    harborMsg_t msg = {0};
    if(errorcode != 0){
        msg.msgId = message->msgId;
        msg.errorcode = errorcode;
        bRet = harbor->callback(harbor,EMSGTYPE_ERROR,&msg);
    }else{
        msg.msgId = message->msgId;
        msg.message = output->buffer.data;
        msg.len = output->buffer.size;
        bRet = harbor->callback(harbor,EMSGTYPE_REMOTE,&msg);
    }
    free(message);

    // If error, we reset the httpC
    if(errorcode!=0){
        httpc_clear(&harbor->httpc,false);
    }

    if(bRet){
        harbor_continue(harbor);
    }
}


static void harbor_continue(harbor_t *harbor)
{
    if(harbor->isBusy||list_empty(&harbor->sendList)){
        return;
    }

    harbor->isBusy = true;
    harbor_message_t *message = list_entry(harbor->sendList.next,harbor_message_t,head);

    bool bRet = httpc_load(&harbor->httpc,
        harbor->szURL,
        HTTP_POST,
        HARBOR_TIMEOUT,
        message->msg,
        message->len,
        EOUT_BUFF,
        NULL,
        harobr_httpHandler,
        (void*)harbor);
    if(!bRet){
        harobr_httpHandler((void*)harbor,NULL,-5);
    }
}

void harbor_send(void* handle, int msgId, char *msg, int len)
{
    harbor_t *harbor = (harbor_t*)handle;
    
    harbor_message_t *message = (harbor_message_t*)malloc(sizeof(harbor_message_t)+len+1);
    memset(message,0x00,sizeof(harbor_message_t)+len+1);
    message->msg = (char*)(message+1);
    message->len = len;
    message->msgId = msgId;
    memcpy(message->msg,msg,len);

    list_add_tail(&message->head,&harbor->sendList);

    harbor_continue(harbor);
}

void* harbor_start(char *szURL, msg_callback callback)
{
    harbor_t *harbor = (harbor_t*)malloc(sizeof(harbor_t));
    memset(harbor,0x00,sizeof(harbor_t));
    harbor->szURL = cmmn_strdup(szURL);
    harbor->callback = callback;
    INIT_LIST_HEAD(&harbor->sendList);
    return harbor;
}

void harbor_stop(void* handle)
{
    harbor_t *harbor = (harbor_t*)handle;

    httpc_clear(&harbor->httpc,false);
    
    struct list_head *pos=NULL,*_next=NULL;
    list_for_each_safe(pos,_next,&harbor->sendList){
        harbor_message_t *message = list_entry(pos,harbor_message_t,head);
        free(message);
    }

    if(harbor->szURL){
        free(harbor->szURL);
        harbor->szURL = NULL;
    }
    free(harbor);
}