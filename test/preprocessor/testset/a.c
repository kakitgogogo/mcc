#include <stdio.h>
#include "a.h"

#include "a.h"
#include "a.h"
#include "a.h"

#define HAHA

#if a+1-1*2+(1^3)-defined(HAHA)
#error "error"
#else
    #if 1
        #if 0
        #elif defined HAHA
            #if 1
                #pragma message "yeah"
            #endif
        #endif
    #endif
#endif

#define x(a, b) a##b

#define str(a) #a

char* s = str(xxxxxx);

int main() {
    int b = 0x1111;
    int x(xx, xx);
}

#error "hahaha"
