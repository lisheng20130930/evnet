#include "evfunclib.h"
#include "stdio.h"
#include "libos.h"
#include "httpc.h"


static int stop = 0;
static void handler_cb(void *pUsr, coutputer_t *output, int errorCode)
{
	if(output->buffer.data)
		printf(output->buffer.data);
	else
		printf("no data");
	stop = true;
}


char* file2buffer(char *pszFileName, int *piLen)
{
    FILE *pFile = NULL;    
    char *pcFileBuff = NULL;
    int iInLen = 0;
    
    pFile = fopen(pszFileName, "rb");    
    if(0 == pFile){
        return NULL;
    }
    
    fseek(pFile, 0, SEEK_END);    
    iInLen = ftell(pFile);
    pcFileBuff = (char*)malloc(iInLen+1);
    memset(pcFileBuff, 0x00, iInLen+1);
    fseek(pFile, 0, SEEK_SET);    
    fread(pcFileBuff,iInLen, 1, pFile);    
    fclose(pFile);
    
    *piLen = iInLen;
    
    return pcFileBuff;
}

int main(int argv, char **argc)
{
	char *pszdata = NULL;
	int len = 0;

	pszdata = file2buffer("request.json",&len);

	evnet_init(3000,500,0);

	httpc_t httpc = {0};
	httpc_load(&httpc,"https://www.baidu.com/",
		HTTP_GET,8,NULL,0,EOUT_BUFF,NULL,handler_cb,NULL);
	while(!stop){
        evnet_loop(0);
    }
    printf("end");
    evnet_uint();

	return 0;
}

