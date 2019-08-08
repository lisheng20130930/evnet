#include "evfunclib.h"
#include "aesocket.h"
#include "event.h"
#include "time.h"
#ifndef WIN32
#include "sys/time.h"
#endif

#if(defined(WIN32)||defined(__IOS__)||defined(__MAC__)||defined(__ANDROID__))
typedef struct aeApiState {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = (aeApiState*)malloc(sizeof(aeApiState));
    memset(state, 0x00, sizeof(aeApiState));

    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    eventLoop->apidata = state;
    return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
    free(eventLoop->apidata);
}

static int aeApiAddEvent(aeEventLoop *eventLoop, fd_t fd, int mask) {
    aeApiState *state = (aeApiState*)eventLoop->apidata;

#ifndef WIN32
    if (mask & AE_READABLE) FD_SET(fd,&state->rfds);
    if (mask & AE_WRITABLE) FD_SET(fd,&state->wfds);
#else
#define EV_FD_TO_WIN32_HANDLE(fd) _get_osfhandle(fd)
#define EV_WIN32_HANDLE_TO_FD(handle) _open_osfhandle(handle, 0)
    if (mask & AE_READABLE) FD_SET((SOCKET)EV_FD_TO_WIN32_HANDLE(fd),&state->rfds);
    if (mask & AE_WRITABLE) FD_SET((SOCKET)EV_FD_TO_WIN32_HANDLE(fd),&state->wfds);
#endif
    return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, fd_t fd, int mask) {
    aeApiState *state = (aeApiState*)eventLoop->apidata;

#ifndef WIN32
    if (mask & AE_READABLE) FD_CLR(fd,&state->rfds);
    if (mask & AE_WRITABLE) FD_CLR(fd,&state->wfds);
#else
    if (mask & AE_READABLE) FD_CLR((SOCKET)EV_FD_TO_WIN32_HANDLE(fd),&state->rfds);
    if (mask & AE_WRITABLE) FD_CLR((SOCKET)EV_FD_TO_WIN32_HANDLE(fd),&state->wfds);
#endif
}

static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = (aeApiState*)eventLoop->apidata;
    int retval = 0, j, numevents = 0;

    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    
    if (retval > 0) {
        for (j = 0; j <= eventLoop->maxfd; j++) {
            int mask = 0;
            aeFileEvent *fe = &eventLoop->events[j];

            if (fe->mask == AE_NONE) continue;
#ifndef WIN32
            if (fe->mask & AE_READABLE && FD_ISSET(j,&state->_rfds))
                mask |= AE_READABLE;
            if (fe->mask & AE_WRITABLE && FD_ISSET(j,&state->_wfds))
                mask |= AE_WRITABLE;
#else
            if (fe->mask & AE_READABLE && FD_ISSET(EV_FD_TO_WIN32_HANDLE(j),&state->_rfds))
                mask |= AE_READABLE;
            if (fe->mask & AE_WRITABLE && FD_ISSET(EV_FD_TO_WIN32_HANDLE(j),&state->_wfds))
                mask |= AE_WRITABLE;
#endif
            eventLoop->fired[numevents].fd = j;
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}

static char* aeApiName(void) {
    return "select";
}
#else
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/epoll.h>

typedef struct aeApiState {
    int epfd;
    struct epoll_event *events;
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = malloc(sizeof(aeApiState));
    state->events = malloc(sizeof(struct epoll_event)*eventLoop->setsize);
    state->epfd = epoll_create(1024); /* 1024 is just an hint for the kernel */
    if (state->epfd == -1) {
        free(state->events);
        free(state);
        return -1;
    }
    eventLoop->apidata = state;
    return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
    aeApiState *state = eventLoop->apidata;

    close(state->epfd);
    free(state->events);
    free(state);
}

static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee;
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = eventLoop->events[fd].mask == AE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* Merge old events */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;
    if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;
    return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee;
    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;
    if (mask != AE_NONE) {
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}

static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}

static char *aeApiName(void) {
    return "epoll";
}
#endif

#if(defined(WIN32)||defined(__IOS__)||defined(__ANDROID__))
#define _TIMEOUT_MS  (0)
#else
#define _TIMEOUT_MS  (20)
#endif

aeEventLoop *aeCreateEventLoop(int setsize)
{
    aeEventLoop *eventLoop = NULL;
    int i = 0;

    eventLoop = (aeEventLoop*)malloc(sizeof(aeEventLoop));    
    eventLoop->events = (aeFileEvent*)malloc(sizeof(aeFileEvent)*setsize);
    eventLoop->fired = (aeFiredEvent*)malloc(sizeof(aeFiredEvent)*setsize);
    eventLoop->setsize = setsize;
    eventLoop->lastTime = (unsigned int)time(NULL);
    eventLoop->maxfd = -1;
    if(aeApiCreate(eventLoop) == -1)
    {
        free(eventLoop->events);
        free(eventLoop->fired);
        free(eventLoop);
        return NULL;
    }
    
    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < setsize; i++)
    {
        eventLoop->events[i].mask = AE_NONE;
    }

    return eventLoop;
}

void aeDeleteEventLoop(aeEventLoop *eventLoop)
{
    aeApiFree(eventLoop);
    free(eventLoop->events);
    free(eventLoop->fired);
    free(eventLoop);
}

int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData)
{
    aeFileEvent *fe = NULL;

    if(fd >= eventLoop->setsize){
        return (-1);
    }
    
    fe = &eventLoop->events[fd];

    if(aeApiAddEvent(eventLoop, fd, mask)==-1){
        return (-1);
    }
    fe->mask |= mask;
    if(mask & AE_READABLE) fe->rfileProc = proc;
    if(mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if(fd > eventLoop->maxfd) eventLoop->maxfd = fd;

    return 0;
}

void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    aeFileEvent *fe = NULL;

    if(fd >= eventLoop->setsize)
    {
        return;
    }
    
    fe = &eventLoop->events[fd];

    if(fe->mask == AE_NONE)
    {
        return;
    }
    fe->mask = fe->mask & (~mask);
    if(fd == eventLoop->maxfd && fe->mask == AE_NONE)
    {
        /* Update the max fd */
        int j = 0;

        for(j = eventLoop->maxfd-1; j >= 0; j--)
        {
            if (eventLoop->events[j].mask != AE_NONE)
            { 
                break;
            }
        }
        eventLoop->maxfd = j;
    }
    aeApiDelEvent(eventLoop, fd, mask);
}

int aeProcessEvents(aeEventLoop *eventLoop)
{
    int processed = 0, numevents;

    /* Note that we want call select() even if there are no
     * file events to process as ts_long_t as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    if (eventLoop->maxfd != -1){
        int j;
        struct timeval tv={0}, *tvp=NULL;
        
        /* If we have to check for events but need to return
         * ASAP because of AE_DONT_WAIT we need to set the timeout
         * to zero */
        tv.tv_sec = 0;
        tv.tv_usec = _TIMEOUT_MS*1000;
        tvp = &tv;
        
        numevents = aeApiPoll(eventLoop, tvp);
        for (j = 0; j < numevents; j++) {
            aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int rfired = 0;

	    /* note the fe->mask & mask & ... code: maybe an already processed
             * event removed an element that fired and we still didn't
             * processed, so we check if the event is still valid. */
            if (fe->mask & mask & AE_READABLE) {
                rfired = 1;
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
            }
            if (fe->mask & mask & AE_WRITABLE) {
                if (!rfired || fe->wfileProc != fe->rfileProc)
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
            }
            processed++;
        }
    }

    return processed; /* return the number of processed file/time events */
}

char* aeGetApiName(void) {
    return aeApiName();
}
