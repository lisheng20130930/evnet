/**
 * This is a module for multi-thread, we use tpool.c/h in utils
 * Wrap-external for evnet-loop
 * You should init/loop/uint it by yourself
 */
#ifndef EVACTOR_H
#define EVACTOR_H


#define NTHERAD  (2)

enum{
    EVACTOR_THREADRUN = 0,
    EVACTOR_DONE
};


typedef void (*evhandler_t)(void *pUsr, int evType);

int evactor_handle(evhandler_t handler, void* pUsr);

void evactor_loop();
void evactor_init();
void evactor_uint();


#endif