/*
 * httpc.h 
 *
 * The http client API for http(s) app client
 *
 * Copyright (C) Listen.Li, 2018
 *
 */
#ifndef HTTP_C_H
#define HTTP_C_H


#include "buffer.h"
#include "httparser.h"


enum{
    EOUT_FILE = 0,
    EOUT_BUFF,
};


typedef union coutputer_s{
    char *pathName; // pathName
    buffer_t buffer; // buffer
}coutputer_t;


typedef void (*pfnHalder_t)(void *pUsr, coutputer_t *output, int errorCode);  //0-success,(-1)-error,(-3)-ignore


typedef struct _httpc_s{
    void *output;
    bool field;
    int  timeout;
    int  total;
    int  size;
    bool complete;
    void *c;
}httpc_t;



bool httpc_load(httpc_t *c, char *szURL, int iMethod, int timeout, char *pszPost, int postLen, int outType, char *pathName, pfnHalder_t pfn, void *pUsr);
void httpc_clear(httpc_t *c, bool bNotify);
int  httpc_percent(httpc_t *c);



#endif