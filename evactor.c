#include "pthread.h"
#include "thpool.h"
#include "evfunclib.h"
#include "libos.h"
#include "buffer.h"
#include "listlib.h"
#include "stdarg.h"
#include "utils.h"
#include "evactor.h"


typedef struct _actor_s{
    struct list_head   head;   //LIST HEAD	
    evhandler_t handler;
    unsigned int id;
    void *pUsr;
}actor_t;


static threadpool gworkers = NULL;

static struct list_head gactorFinished = {&gactorFinished,&gactorFinished};  /* finished actor here */
static pthread_mutex_t gmutex; /* used for finished r/w access */

static unsigned int gid = 0;


static actor_t* actor_new(int id, void *pUsr, evhandler_t handler)
{
    actor_t *pactor = (actor_t*)malloc(sizeof(actor_t));
    memset(pactor,0x00,sizeof(actor_t));	
    pactor->handler = handler;
    pactor->pUsr = pUsr;
    pactor->id = id;
    return pactor;
}

static void actor_free(actor_t *pactor)
{
    free(pactor);
}

static void actor_run(void *actor)
{	
    actor_t *pactor = (actor_t*)actor;  
    pactor->handler(pactor->pUsr,EVACTOR_THREADRUN);
    pthread_mutex_lock(&gmutex);
    list_add_tail(&pactor->head,&gactorFinished);
    pthread_mutex_unlock(&gmutex);    
    evnet_async();
}


int evactor_handle(evhandler_t handler, void* pUsr)
{        
    actor_t *pactor = actor_new(++gid, pUsr, handler);
    DBGPRINT(EERROR,("[EvActor] evactor_handle (id=%d) called======\r\n",gid));
    return thpool_add_work(gworkers,actor_run,pactor);
}

void evactor_loop()
{
    #define MAX_TCB  (128)
    actor_t *arr[MAX_TCB] = {0};  // MAX_actor every time
    int i=0,n=0;
    
    if(list_empty(&gactorFinished)){
        return;
    }
    
    struct list_head *pos=NULL,*_next=NULL;
    pthread_mutex_lock(&gmutex);
    list_for_each_safe(pos,_next,&gactorFinished){
        list_del(pos); //DEL
        actor_t *pactor = list_entry(pos,actor_t,head);
        arr[n++] = pactor;
        if(n>=MAX_TCB){
            break;
        }
    }
    pthread_mutex_unlock(&gmutex);
    
    for(i=0;i<n;i++){
        actor_t *pactor = arr[i];
        DBGPRINT(EERROR,("[EvActor] finished Leave, id=%d\r\n",pactor->id));
        pactor->handler(pactor->pUsr,EVACTOR_DONE);
        actor_free(pactor);
    }
    
    if(n>=MAX_TCB){
        evnet_async();
    }
}

void evactor_init()
{
    pthread_mutex_init(&gmutex, NULL);
    gworkers = thpool_init(NTHERAD);
}

void evactor_uint()
{
    thpool_destroy(gworkers);
    pthread_mutex_destroy(&gmutex);
    gworkers = NULL;
}