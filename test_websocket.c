#include "libos.h"
#include "evfunclib.h"
#include "harbor.h"


int maindfs(int argc, char **argv)
{
    evnet_init(3000);    
    unsigned int g_loop = 1;    
    
    while(1){        
        evnet_loop(g_loop);
        g_loop++;
    }
    evnet_uint();
    
    return 0;
}