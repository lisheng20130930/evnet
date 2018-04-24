#include "dataqueue.h"

void dataqueue_remove_data(dataqueue_t *queue, int bFreeMemroy)
{
    queue->dataSize = 0;
    queue->insertPos = 0;
    queue->terminalPos = 0;
    queue->queryPos = 0;
    
    if(bFreeMemroy){
        queue->bufferSize=0;
        free(queue->m_pDataQueueBuffer);
        queue->m_pDataQueueBuffer=NULL;
    }
}

void dataqueue_insert_data(dataqueue_t *queue, void *pBuffer, int wDataSize)
{
    unsigned int dwCopySize = wDataSize;
    int excp = 0;
    
    if(queue->dataSize+dwCopySize > queue->bufferSize){
        excp = 1;
    }
    else if(queue->insertPos==queue->terminalPos
        &&(queue->insertPos+dwCopySize)>queue->bufferSize){
        if(queue->queryPos >= dwCopySize){
            queue->insertPos = 0;
        }
        else{
            excp = 1;
        }
    }
    else{
        if(queue->insertPos<queue->terminalPos
            &&(queue->insertPos+dwCopySize)>queue->queryPos){
            excp = 1;
        }
    }
    
    if(excp){
        unsigned int dwNewBufferSize = 0;
        char *pNewQueueServiceBuffer = NULL;
        unsigned int dwPartOneSize = 0;

        dwNewBufferSize = queue->bufferSize + __max(queue->bufferSize/2,(unsigned int)wDataSize);
        pNewQueueServiceBuffer = (char*)malloc(dwNewBufferSize);		
        memset(pNewQueueServiceBuffer, 0x00, dwNewBufferSize);
                
        if(NULL != queue->m_pDataQueueBuffer){
            dwPartOneSize = queue->terminalPos - queue->queryPos;
            if(dwPartOneSize > 0){
                memcpy(pNewQueueServiceBuffer, queue->m_pDataQueueBuffer+queue->queryPos,dwPartOneSize);
            }
            if(queue->dataSize > dwPartOneSize){                
                memcpy(pNewQueueServiceBuffer+dwPartOneSize,queue->m_pDataQueueBuffer,queue->insertPos);
            }
        }
        
        queue->queryPos = 0;
        queue->insertPos = queue->dataSize;
        queue->terminalPos = queue->dataSize;
        queue->bufferSize = dwNewBufferSize;
        if(NULL != queue->m_pDataQueueBuffer){
            free(queue->m_pDataQueueBuffer);
        }
        queue->m_pDataQueueBuffer = pNewQueueServiceBuffer;
    }
    
    memcpy(queue->m_pDataQueueBuffer+queue->insertPos, pBuffer, wDataSize);   
    queue->dataSize += dwCopySize;
    queue->insertPos += dwCopySize;
    queue->terminalPos = __max(queue->terminalPos,queue->insertPos);
}

void dataqueue_distill_data(dataqueue_t *queue, int wDataSize)
{
    if(0 == queue->dataSize||!queue->m_pDataQueueBuffer){
        return;
    }    
    if(queue->queryPos == queue->terminalPos){
        queue->queryPos = 0;
        queue->terminalPos = queue->insertPos;
    }        
    queue->dataSize -= wDataSize;
    queue->queryPos += wDataSize;
}

void dataqueue_next_data(dataqueue_t *queue, char **ppData, int *pLen)
{
    if(queue->queryPos == queue->terminalPos){
        queue->queryPos = 0;
        queue->terminalPos = queue->insertPos;
    }

    *ppData = queue->m_pDataQueueBuffer+queue->queryPos;
    *pLen = queue->terminalPos-queue->queryPos;
}
