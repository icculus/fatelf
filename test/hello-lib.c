#include <stdio.h>

#if defined(__i386__)
#define cpu "x86"
#elif defined(__x86_64__)
#define cpu "x86-64"
#elif defined(__powerpc64__)
#define cpu "powerpc64"
#elif defined(__powerpc__)
#define cpu "powerpc"
#else
#error Please add your CPU architecture here.
#endif

int hello(void) 
{
    printf("hello from a shared library! This binary's arch is: %s!\n", cpu);
    return 0;
}

/* end of hello-lib.c ... */

