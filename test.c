#include "evfunclib.h"
#include "stdio.h"


static int g_counter = 0;


#if 0
static int _upstream_callback(void *pUser, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
    void *downstream = evnet_channeluser(msgChannel->channel);
    char *buffer = NULL;
    int len = 0;
    
    switch(msgChannel->identify){
    case _EVDATA:
        while(dataqueue_datasize(msgChannel->u.dataqueue)){
            dataqueue_next_data(msgChannel->u.dataqueue, &buffer, &len);
            evnet_channelsend(downstream, buffer, len);
            dataqueue_distill_data(msgChannel->u.dataqueue, len);
        }
        break;
    case _EVCLOSED:
        if((-8)==msgChannel->u.errcode){
            break;
        }
        evnet_closechannel(downstream, (-200));
        break;
    default:
        break;
    }
    return 0;
}
#endif

static int _channel_callback(void *pUser, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
    //void *upstream = evnet_channeluser(msgChannel->channel);
    //char *buffer = NULL;
    //int len = 0;

    switch(msgChannel->identify){
    case _EVDATA:
        //while(dataqueue_datasize(msgChannel->u.dataqueue)){
         //   dataqueue_next_data(msgChannel->u.dataqueue, &buffer, &len);
         //   evnet_channelsend(upstream, buffer, len);
        //    dataqueue_distill_data(msgChannel->u.dataqueue, len);
        //}
        break;
    case _EVCLOSED:
        //if((-200)!=msgChannel->u.errcode){
        //    evnet_closechannel(upstream, (-8));
        //}
        g_counter--;
        printf("COUNTER: %d\r\n", g_counter);
        break;
    default:
        break;
    }
    return 0;
}


static int _acceptor_callback(void *pUser, void *msg, unsigned int size)
{
    msgAcceptor_t *msgAcceptor = (msgAcceptor_t*)msg;
    //void *upstream = NULL;

    //upstream = evnet_createchannel(evnet_hostbyname("222.46.20.106"), 22000);
    //evnet_channelbind(upstream, _upstream_callback, 65535, msgAcceptor->u.channel);
    evnet_channelbind(msgAcceptor->u.channel, _channel_callback, 65535, 0);

    g_counter++;
    printf("COUNTER: %d\r\n", g_counter);
    return 0;
}


int main(int argv, char **argc)
{
    void* acceptor = NULL;

    evnet_init(2000);
    acceptor = evnet_createacceptor(12000, 0, NULL, _acceptor_callback, NULL);
    evnet_acceptorstart(acceptor);
    while(1){
        evnet_loop(0);
    }
    printf("end");
    evnet_uint();

	return 0;
}
