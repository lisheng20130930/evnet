/* Code From Redis */
#ifndef OV_EVENT_H
#define OV_EVENT_H

#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2

struct aeEventLoop;
typedef void aeFileProc(struct aeEventLoop *eventLoop, fd_t fd, void *clientData, int mask);

/* File event structure */
typedef struct aeFileEvent {
    int mask; /* one of AE_(READABLE|WRITABLE) */
    aeFileProc *rfileProc;
    aeFileProc *wfileProc;
    void *clientData;
} aeFileEvent;

/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;

/* State of an event based program */
typedef struct aeEventLoop {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    unsigned int lastTime;     /* Used to detect system clock skew */
    aeFileEvent *events; /* Registered events */
    aeFiredEvent *fired; /* Fired events */    
    void *apidata; /* This is used for polling API specific data */
}aeEventLoop;


aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeProcessEvents(aeEventLoop *eventLoop);
char* aeGetApiName(void);


#endif
