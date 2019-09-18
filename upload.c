#include "libos.h"
#include "httpd.h"
#include "upload.h"


//upload dir can be changed as your need
static char* getUploadDir()
{
	static char g_upload_dir[64]={0};
	memset(g_upload_dir,0x00,64);
	#ifdef WIN32
	g_upload_dir[0] = '.';
	#endif
	strcat(g_upload_dir,"./upload");
	return g_upload_dir;
}


char* upload_pathName(char *name)
{
	static char g_pathname[1024] = {0};
	sprintf(g_pathname,"%s/%s",getUploadDir(),name);
	return g_pathname;
}

static int read_header_name(multipart_parser *p, const char *at, size_t length)
{
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	header_t *hd = (header_t*)malloc(sizeof(header_t));
	memset(hd,0x00,sizeof(header_t));
	hd->fieldname = cmmn_strndup(at,(int)length);
	list_add(&hd->hd,&node->req.upload.mhdr);
    return 0;
}

static int read_header_value(multipart_parser *p, const char *at, size_t length)
{
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	header_t *hd = list_entry(node->req.upload.mhdr.next,header_t,hd);
	if(hd){
		hd->fieldvalue = cmmn_strndup(at,(int)length);
	}
    return 0;
}

static int on_part_data(multipart_parser *p, const char *at, size_t length)
{    
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	if(node->req.upload.pFile){
		fwrite(at,length,1,node->req.upload.pFile);
	}
    return 0;
}

static int on_headers_complete(multipart_parser *p)
{
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	struct list_head *pos=NULL;
	char *contentValue = NULL;
	
	list_for_each(pos,&node->req.upload.mhdr){
		header_t *hd = list_entry(pos,header_t,hd);
		if(!stricmp(hd->fieldname,"Content-Disposition")){
			contentValue = hd->fieldvalue;
			break;
		}
	}

	if(!contentValue){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. NOT Found Content-Disposition\r\n"));
		return (-1);
	}
	
	DBGPRINT(EERROR,("[Trace@UPLOAD] Content-Disposition:%s\r\n", contentValue));
	char *pST = strstr(contentValue,"filename=\"");
	char *pED = NULL;
	if(!pST){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. NOT Found filename\r\n"));
		return (-1);
	}
	pST+=10; //skip
	pED = strstr(pST,"\"");
	if(!pED || pED-pST>=sizeof(node->req.upload.filename)){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. name too long\r\n"));
		return (-1);
	}

	memset(node->req.upload.filename,0x00,sizeof(node->req.upload.filename));
	memcpy(node->req.upload.filename,pST,pED-pST);
	
	char *pathname = fmt2("%s/%s",getUploadDir(),node->req.upload.filename);
	remove(pathname); //force remove IF Has

	node->req.upload.pFile = fopen(pathname,"ab+");
    if(!node->req.upload.pFile){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. Open Failed\r\n"));
		return (-1);
    }
	
    return 0;
}

static int on_part_data_end(multipart_parser *p)
{
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	if(node->req.upload.pFile){
		fclose(node->req.upload.pFile);
		node->req.upload.pFile = NULL;
	}
    return 0;
}

static int on_body_end(multipart_parser *p)
{
	struct node_s *node = (struct node_s*)multipart_parser_get_data(p);
	node->req.upload.isComplete = true;
    return 0;
}

static multipart_parser_settings parser_body_settings = 
{
	read_header_name,	
    read_header_value,
    on_part_data,
	NULL,
	on_headers_complete,
    on_part_data_end,
    on_body_end,
};



bool upload_handleHeader(struct node_s *node)
{
	struct list_head *pos=NULL;
	char *contentValue = NULL;

	list_for_each(pos,&node->headers){
		header_t *hd = list_entry(pos,header_t,hd);
		if(!stricmp(hd->fieldname,"Content-Type")){
			contentValue = hd->fieldvalue;
			break;
		}
	}

	if(!contentValue){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. NULL contentType\r\n"));
		return false;
	}

	DBGPRINT(EERROR,("[Trace@UPLOAD] content type:%s\r\n", contentValue));
    if(!strstr(contentValue, "multipart/form-data;")){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. not match content-type Value\r\n"));
		return false;
	}

	char *p = strstr(contentValue, "boundary");
	if (!p){
		DBGPRINT(EERROR,("[Trace@UPLOAD] Error. NOt found boundary\r\n"));
		return false;
	}

	char boundary[128] = {0};
	strcpy(boundary, "--");
	strcat(boundary, p+9);
	printf("%s\n", boundary);

    node->req.upload.mp = multipart_parser_init(boundary, &parser_body_settings);
	multipart_parser_set_data(node->req.upload.mp,node);
    INIT_LIST_HEAD(&node->req.upload.mhdr);
	node->req.upload.isComplete = false;
	node->req.upload.pFile = NULL;

	return true;
}

bool upload_checkURL(struct node_s *node)
{
	if(!strstr(node->URL,"/upload")){
		DBGPRINT(EERROR,("[Trace@UPLOAD] NOT upload Path. URL=%s\r\n",node->URL));
		return false;
	}
	return true;
}

int upload_bodyContinue(struct node_s *node, char *data, int len)
{
	return multipart_parser_execute(node->req.upload.mp,data,len);
}

void upload_clear(node_t *_node)
{
	struct node_s *node = (struct node_s*)_node;
	if(node->req.upload.mp){
		multipart_parser_free(node->req.upload.mp);
		node->req.upload.mp = NULL;
	}
	if(node->req.upload.pFile){
		fclose(node->req.upload.pFile);
		node->req.upload.pFile = NULL;
	}
}