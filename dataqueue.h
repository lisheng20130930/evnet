#ifndef DATAQUEUE_H
#define DATAQUEUE_H


#include "stdlib.h"
#include "memory.h"
#include "stdbool.h"


#ifndef __max
#define __max(a,b)  (((a) > (b)) ? (a) : (b))
#endif


typedef struct dataqueue_s
{    
    unsigned int insertPos;
    unsigned int terminalPos;
    unsigned int queryPos;
    unsigned int dataSize;
    unsigned int bufferSize;
    char *m_pDataQueueBuffer;
}dataqueue_t;

#define dataqueue_uinit(p) dataqueue_remove_data(p,1)
#define dataqueue_datasize(p) ((p)->dataSize)

void dataqueue_remove_data(dataqueue_t *queue, int bFreeMemroy);
void dataqueue_insert_data(dataqueue_t *queue, void *pBuffer, int wDataSize);
void dataqueue_distill_data(dataqueue_t *queue, int wDataSize);
void dataqueue_next_data(dataqueue_t *queue, char **ppData, int *pLen);


#endif
