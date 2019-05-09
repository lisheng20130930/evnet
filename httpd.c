#include "libos.h"
#include "evfunclib.h"
#include "httpd.h"
#include "listlib.h"


typedef struct _usr_s{
	list_head head;  //LISTHEAD
	void *channel;			// channel
	void *node;				// 当前的请求
	void *httpd;
	unsigned int dwUsrID;   // usrID
}usr_t;


typedef struct _httpd_s{
	node_handle_t handler;
	int nodesize;
	int timeout;
	int maxCon;
	void *acceptor;
	int curCon;
	list_head usr_list; //UsrList
}httpd_t;


static int node_c_on_message_begin(http_parser *parser)
{
    struct node_s *node = (struct node_s*)parser->data;    
	
	DBGPRINT(EDEBUG,("[HTTPD] dwUsrID= %d Http-Method=%d\r\n",((usr_t*)node->usr)->dwUsrID,parser->method));
    if(HTTP_POST!=parser->method&&HTTP_GET!=parser->method){
        return (-1);
    }
	node->method = parser->method;
    return 0;
}

static int node_c_on_url(http_parser *parser, const char *at, int length)
{
    struct node_s *node = (struct node_s*)parser->data;    
	node->URL = cmmn_strndup(at, length);
    return 0;
}

static int node_c_on_header_field(http_parser *parser, const char *at, int length)
{
	struct node_s *node = (struct node_s*)parser->data;
	header_t *hd = (header_t*)malloc(sizeof(header_t));
	memset(hd,0x00,sizeof(header_t));
	hd->fieldname = cmmn_strndup(at,length);
	list_add_head(&node->headers,hd);
	return 0;
}

static int node_c_on_header_value(http_parser *parser, const char *at, int length)
{
	struct node_s *node = (struct node_s*)parser->data;
	header_t *hd = (header_t*)(node->headers.next);
	if(hd){
		hd->fieldvalue = cmmn_strndup(at,length);
	}
	return 0;
}

static int node_c_on_body(http_parser *parser, const char *at, int length)
{
    struct node_s *node = (struct node_s*)parser->data;	
    buffer_append(&node->buffer, (char*)at, length); 
	DBGPRINT(EDEBUG,("[HTTPD] dwUsrID=%d,Http-Body-Curr-Size=%d\r\n",((usr_t*)node->usr)->dwUsrID,node->buffer.size));
    return 0;
}

static int node_c_on_message_complete(http_parser *parser)
{
    struct node_s *node = (struct node_s*)parser->data;
    node->status = estatnodeeady;    
    return 0;
}

static const http_parser_settings c_http_parser_settings = 
{
    node_c_on_message_begin,
	node_c_on_url,
	NULL,
	node_c_on_header_field,
	node_c_on_header_value,
	NULL,
	node_c_on_body,
	node_c_on_message_complete,
};

static int _node_parser(struct node_s *node, char *buffer, int len, int *pos)
{
    int usedlen = 0;    
    /* parser */
    node->parser.data = (void*)node;
    
    /* parser execute */
    usedlen = http_parser_execute(&node->parser, &c_http_parser_settings, buffer, len);
    if(estatnodeeady==node->status){
        /* success */
        *pos = usedlen;
        return 0;
    }
    
    /* if is inilizing and data all parsed, we need more data */ 
    if(usedlen == len){
        *pos = len;
        return (-1);
    }
	
	DBGPRINT(EERROR,("[HTTPD] _node_parser Failure!!!!\r\n"));	
    return 0;
}


static void node_free(struct node_s *node)
{
	list_head *pos=NULL,*_next=NULL;
	list_for_each_safe(pos,_next,&node->headers){
		header_t *hd = list_entry(pos,header_t);
		free(hd->fieldname);
		free(hd->fieldvalue);
		free(hd);
	}
	if(node->URL){
		free(node->URL);
	}
	free(node);
}


static struct node_s* _uncomplete_node(httpd_t *httpD, void* c)
{
	usr_t *usr = (usr_t*)evnet_channeluser(c);
    struct node_s *node = NULL;
	
	if(usr->node){
		node = (struct node_s*)usr->node;
		if(node->status==estatusnone){
			return node;
		}
		DBGPRINT(EERROR,("[HTTPD] already Has node, Error!\r\n"));	
		return NULL;
	}
	node = (struct node_s*)malloc(httpD->nodesize);
	memset(node, 0x00, httpD->nodesize);
	init_list_head(&node->headers);
	node->usr = usr;
	http_parser_init(&node->parser, HTTP_REQUEST);
	buffer_init(&node->buffer);
	
	usr->node = node;
    return node;
}

static void _channelClose(httpd_t *httpD, void* c)
{
    usr_t *usr = (usr_t*)evnet_channeluser(c);
    node_t *node = NULL;
	
	DBGPRINT(EDEBUG,("[HTTPD] usrClose dwUsrID=%u,node=0x%x\r\n",usr->dwUsrID,usr->node));
	
	if(usr->node){
		node=(node_t*)usr->node;
		httpD->handler(node,ENODECLEAR,0,0);
		node_free((struct node_s*)node);
	}
	free(usr);    
}

static void node_send(node_t *_node, char *data, int size, int isLastPacket)
{
	struct node_s *node = (struct node_s*)_node;
	usr_t *usr = (usr_t*)node->usr;	
	evnet_channelsend(usr->channel, data, size);
	node->sendsize += size;
	if(!isLastPacket){
		return;
	}
	node->status = estatusdone;
}

char* getFiledvalue(node_t *_node, char *fieldname)
{
	struct node_s *node = (struct node_s*)_node;
	list_head *pos=NULL;
	list_for_each(pos,&node->headers){
		header_t *hd = list_entry(pos,header_t);
		if(!stricmp(hd->fieldname,fieldname)){
			return hd->fieldvalue;
		}
	}
	return NULL;
}

char* getRemoteAddr(node_t *node)
{
	char *str = getFiledvalue(node, "X-Real-IP");
	if(!str||strlen(str)==0||!stricmp("unknown",str)){
		str = getFiledvalue(node, "x-forwarded-for");
	}
	if(!str||strlen(str)==0||!stricmp("unknown",str)){
		str = getFiledvalue(node, "Proxy-Client-IP");
	}
	if(!str||strlen(str)==0||!stricmp("unknown",str)){
		str = getFiledvalue(node, "WL-Proxy-Client-IP");
	}
	if(!str||strlen(str)==0||!stricmp("unknown",str)){
		str = NULL;
	}
	
	static char ip[IPSTRSIZE] = {0};
	memset(ip,0x00,IPSTRSIZE);
	
	if(str){
		char *pEd = strchr(str,',');
		size_t len = pEd?pEd-str:strlen(str);
		memcpy(ip,str,__min(len,IPSTRSIZE-1));
	}else{
		strcpy(ip,evnet_channelip(((usr_t*)((struct node_s*)node)->usr)->channel));
	}
	
	return ip;
}

static void node_continue(httpd_t *httpD, struct node_s *node)
{
	DBGPRINT(EERROR,("[HTTPD] Continue Node! dwUsrID=%u IP(%s) URL=%s\r\n",((usr_t*)node->usr)->dwUsrID,getRemoteAddr((node_t*)node),node->URL));
	node->status=estatushandle;
	node->pfnSend = node_send;
	httpD->handler((node_t*)node,ENODEHANDLE,0,0);
}

static int _channel_callback(void *pUser, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
	usr_t *usr = (usr_t*)pUser;
	httpd_t *httpD = (httpd_t*)usr->httpd;
	
    switch(msgChannel->identify){
    case _EVDATA:
		DBGPRINT(EDEBUG,("[HTTPD] __EVDATA-SIZE=%d\r\n",dataqueue_datasize(msgChannel->u.dataqueue)));
        while(0<dataqueue_datasize(msgChannel->u.dataqueue)){
			struct node_s *node = _uncomplete_node(httpD,msgChannel->channel);
			// cannot find uncomplete node
			if(!node){
				return 0;
			}
			char *buffer = NULL;
			int len = 0;
			int pos = 0;
            dataqueue_next_data(msgChannel->u.dataqueue, &buffer, &len);
            if(0==_node_parser(node, buffer, len, &pos)){					
				if(node->status!=estatnodeeady){
					DBGPRINT(EERROR,("[HTTPD] Error dwUsrID=%d\r\n",usr->dwUsrID));
					evnet_closechannel(msgChannel->channel,0);
					return 0;
				}
				dataqueue_distill_data(msgChannel->u.dataqueue,pos);
				/* ready */
				node_continue(httpD,node);
				return 0;				
            }
			/* if pos is zero, means need more data */
			if(pos <= 0){
				break;
			}
			dataqueue_distill_data(msgChannel->u.dataqueue,pos);
        }
        break;
	case _EVSENT:
		if(NULL!=usr->node){
			struct node_s *node = (struct node_s*)usr->node;
			node->sentsize += msgChannel->u.size; // sent
			DBGPRINT(EDEBUG,("[HTTPD] [sendsize=%u,sentsize=%d], dwUsrID=%u\r\n", node->sendsize,node->sentsize,usr->dwUsrID));
			if(node->sendsize!=node->sentsize){
				DBGPRINT(EMSG,("[HTTPD] Waiting SEnd [sendsize=%u,sentsize=%d], dwUsrID=%u\r\n", node->sendsize,node->sentsize,usr->dwUsrID));
				break;
			}
			if(node->status==estatushandle){				
				DBGPRINT(EDEBUG,("[HTTPD] Continue2 Node! dwUsrID=%u\r\n",usr->dwUsrID));
				httpD->handler((node_t*)node,ENODECONTINUE,msgChannel->u.size,0);
			}else if(node->status==estatusdone){				
				DBGPRINT(EMSG,("[HTTPD] Finish Node! dwUsrID=%u\r\n",usr->dwUsrID));
				usr->node = NULL;
				httpD->handler((node_t*)node,ENODECLEAR,0,0);
				node_free(node);
			}else{
				DBGPRINT(EERROR,("[HTTPD] _EVSENT ERROR!! (badSTatus=%d) dwUsrID=%u\r\n",node->status,usr->dwUsrID));
			}
		}else{
			DBGPRINT(EERROR,("[HTTPD] _EVSENT ERROR!! NO NODE FOUND, dwUsrID=%u\r\n",usr->dwUsrID));
		}
		break;
    case _EVCLOSED:
		list_del(usr); //Remove
		httpD->curCon--;
		_channelClose(httpD,msgChannel->channel);
        DBGPRINT(EMSG,("[HTTPD] EVclosed...COUNTER: %d\r\n", httpD->curCon));
        break;
    default:
        break;
    }
    return 0;
}

static int _acceptor_callback(void *pUser, void *msg, unsigned int size)
{
    msgAcceptor_t *msgAcceptor = (msgAcceptor_t*)msg;
	httpd_t *httpD = (httpd_t*)pUser;
	static int g_ID = 0;
	usr_t *usr = NULL;
	
	if(httpD->curCon>=httpD->maxCon){
		DBGPRINT(EERROR,("[HTTPD] Too much Connect curCon=%d\r\n",httpD->curCon));
		evnet_closechannel(msgAcceptor->u.channel,0);
		return 0;
	}
	
	usr = (usr_t*)malloc(sizeof(usr_t));
	memset(usr,0x00,sizeof(usr_t));
	usr->channel = msgAcceptor->u.channel;
	usr->httpd = httpD;
	usr->dwUsrID = g_ID++;
	
    evnet_channelbind(msgAcceptor->u.channel, _channel_callback,httpD->timeout,(void*)usr);
    
	list_add_tail(&httpD->usr_list,usr);
	httpD->curCon++;
	DBGPRINT(EMSG,("[HTTPD] usrAccepted...dwUsrID=%u COUNTER=%d\r\n", usr->dwUsrID,httpD->curCon));
    return 0;
}


void* httpd_start(node_handle_t handler, int nodesize, unsigned short port, int maxCon, int timeout, int secrity, char *cert)
{
	httpd_t *httpD = (httpd_t*)malloc(sizeof(httpd_t));
	memset(httpD,0x00,sizeof(httpd_t));
	init_list_head(&httpD->usr_list);
	httpD->handler = handler;
	httpD->maxCon = maxCon;
	httpD->timeout = timeout;
	httpD->nodesize = nodesize;
	httpD->acceptor = evnet_createacceptor(port, secrity, cert, _acceptor_callback, httpD);
    if(!httpD->acceptor){
		free(httpD);
		return NULL;
    }
	evnet_acceptorstart(httpD->acceptor);
	return httpD;
}

void httpd_stop(void *httpd)
{
	httpd_t *httpD = (httpd_t*)httpd;
	list_head *pos=NULL,*_next=NULL;
	list_for_each_safe(pos,_next,&httpD->usr_list){
		usr_t *usr = list_entry(pos,usr_t);
		if(usr->channel){
			evnet_closechannel(usr->channel,0);
		}
	}
	evnet_acceptorstop(httpD->acceptor);
	evnet_destroyacceptor(httpD->acceptor);
	free(httpD);
}

