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

    /* have a buffer bigger than a page, so we can maybe find out
       if we put the data segment in the wrong place. */
    int i;
    static char buffer[128 * 1024];
    for (i = 0; i < sizeof (buffer); i++)
        buffer[i] = (char) i;

    return 0;
}

/* end of hello-lib.c ... */

