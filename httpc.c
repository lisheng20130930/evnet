#include "libos.h"
#include "evfunclib.h"
#include "httpc.h"


typedef struct soutputer_s{
	coutputer_t stOutput;
	http_parser parser;
	int outType;
	pfnHalder_t pfn;
	void *pUsr;
	union{
		FILE *pFile; //нд╪Ч╬Д╠З
		int max;
	}u;
}soutputer_t;



static int _up_on_status_complete(http_parser *parser)
{
	httpc_t *httpC = (httpc_t*)parser->data;
	if(200!=parser->status_code){
		return (-1);
	}
    return 0;
}

static int _up_on_header_field(http_parser *parser, const char *at, int length)
{
	httpc_t *httpC = (httpc_t*)parser->data;
	httpC->field = (0==cmmn_strincmp((char*)at, (char*)"Content-Length",length))?(true):(false);
	return 0;
}

static int _up_on_header_value(http_parser *parser, const char *at, int length)
{
	httpc_t *httpC = (httpc_t*)parser->data;
	if (httpC->field){
		httpC->total = natoi((char*)at,length);
	}
	return 0;
}

static void DA_output(httpc_t *httpC, char* str, int size)
{	
	soutputer_t *output = (soutputer_t*)httpC->output;
	if(output->outType==EOUT_FILE){
		if(NULL==output->u.pFile){
			remove(fmt2("%s.dt",output->stOutput.pathName));
			output->u.pFile = fopen(fmt2("%s.dt",output->stOutput.pathName), "ab+");
		}
		if(output->u.pFile){
			fwrite(str,1,size,output->u.pFile);
		}
	}else{
		buffer_append(&output->stOutput.buffer,str,size);
	}
}

static int _up_on_body(http_parser *parser, const char *at, int length)
{
	httpc_t *httpC = (httpc_t*)parser->data;
    if(at&&length>0){
		DA_output(httpC,(char*)at,length);	
		httpC->size += length;
	}
    return 0;
}

static int _up_on_message_complete(http_parser *parser)
{
	httpc_t *httpC = (httpc_t*)parser->data;
    httpC->complete = true;
    return 0;
}

static const http_parser_settings up_http_parser_settings = 
{
    NULL,
	NULL,
	_up_on_status_complete,
	_up_on_header_field,
	_up_on_header_value,
	NULL,
	_up_on_body,
	_up_on_message_complete,
};

static void DA_httpComplete(httpc_t *httpC)
{	
	soutputer_t *output = (soutputer_t*)httpC->output;
	int code = httpC->complete?0:(-1);
	
	http_parser_init(&output->parser,HTTP_REQUEST);	
	httpC->output = NULL;
	httpC->complete = 0;
	
	if(output){
		if(output->outType==EOUT_FILE&&output->u.pFile){ //close file handle	
			fclose(output->u.pFile);
			output->u.pFile = NULL;	
			if (0==code){
				remove(output->stOutput.pathName);
				rename(fmt2("%s.dt",output->stOutput.pathName), output->stOutput.pathName);
			}
			else{
				remove(fmt2("%s.dt",output->stOutput.pathName));
			}
		}else if(output->outType==EOUT_BUFF){
			output->u.max = 0;
		}
	}

	if(output->pfn){
		output->pfn(output->pUsr,&output->stOutput,code);
	}
		
	if(output->outType==EOUT_BUFF){
		buffer_deinit(&output->stOutput.buffer);
	}else{
		free(output->stOutput.pathName);
	}	
	free(output);
}

static int _channel_callback(void *pUser, void *msg, unsigned int size)
{
    msgChannel_t *msgChannel = (msgChannel_t*)msg;
	httpc_t *httpC = (httpc_t*)pUser;
	char *p = NULL;
	int len = 0;
	bool bIsError = false;
	
    switch(msgChannel->identify){
    case _EVDATA:
		while(0<dataqueue_datasize(msgChannel->u.dataqueue)){
			dataqueue_next_data(msgChannel->u.dataqueue, &p, &len);				
			/* parser */
			((soutputer_t*)httpC->output)->parser.data = (void*)httpC;
			/* parser execute */
			if(len!=http_parser_execute(&((soutputer_t*)httpC->output)->parser,&up_http_parser_settings,p,len)){
				bIsError = true;
				break;
			}
			dataqueue_distill_data(msgChannel->u.dataqueue,len);
			len = 0;
		}

		// we dont close the channel,reuse it IF can
		if(httpC->complete){
			DA_httpComplete(httpC);
		}else if(bIsError){
			evnet_closechannel(httpC->c,0);
		}
        break;
    case _EVCLOSED:
		if(httpC->c){
			httpC->c = NULL;
			if(httpC->output){
				DA_httpComplete(httpC);
			}
		}
        break;
    default:
        break;
    }
    return 0;
}

static bool httpc_request(httpc_t *httpC, char *pszURL, char *postData, int postLen, char *pszHeader)
{
	char ipStr[IPSTRSIZE] = {0}; 
	unsigned short port = 0;
	int secrity = 0;
	
	evnet_hostbyname(we_url_host(pszURL),ipStr,IPSTRSIZE);
	secrity = strstr(pszURL,"https://")?1:0; //CHECK SSL
	port = we_url_port(pszURL,secrity?443:80); //CHECK PORT
	
	//check same ip and port
	void *c = httpC->c;
	httpC->c = NULL;
	if(NULL!=c&&(!strcmp(ipStr,evnet_channelip(c))||port!=evnet_channelport(c))){
		evnet_closechannel(c,0);
		c = NULL;
	}
	
	if(NULL==c){
		c = evnet_createchannel((char*)ipStr,port,secrity);
		if (NULL==c) {
			return false;
		}		
		if (!evnet_channelbind(c,_channel_callback,httpC->timeout,(void*)httpC)) {
			evnet_closechannel(c,0);
			return false;
		}
	}
	
    http_parser_init(&((soutputer_t*)httpC->output)->parser, HTTP_RESPONSE);
    /* assign */
    httpC->c = c;
    /* send */
    evnet_channelsend(httpC->c,pszHeader,strlen(pszHeader));
	/* FF post */
	if(postData&&postLen>0){
		evnet_channelsend(httpC->c,postData,postLen);
	}
	return true;
}

bool httpc_load(httpc_t *httpC, char *szURL, int iMethod, int timeout, char *pszPost, int postLen, int outType, char *pathName, pfnHalder_t pfn, void *pUsr)
{	
	soutputer_t *output = (soutputer_t*)malloc(sizeof(soutputer_t));
	memset(output,0x00,sizeof(soutputer_t));
	output->outType = outType;
	output->pfn = pfn;
	output->pUsr = pUsr;

	httpC->field = false;
	httpC->complete = false;
	httpC->size = 0;
	httpC->total = 0;
	httpC->output = output;
	httpC->timeout = timeout;
	
	if(output->outType==0){
		output->stOutput.pathName = cmmn_strdup(pathName);
		remove(pathName);
	}
	
	char *pszHeader = we_url_make_header(iMethod,szURL,postLen);
	bool bRet = httpc_request(httpC,szURL,pszPost,postLen,pszHeader);
	free(pszHeader);

	return bRet;
}

void httpc_clear(httpc_t *httpC)
{
	if(httpC->c){
		evnet_closechannel(httpC->c,0);
	}
}

int httpc_percent(httpc_t *httpC)
{
	if (httpC->total > 0){
		return (int)(100*httpC->size / httpC->total);
	}
	return 0;
}