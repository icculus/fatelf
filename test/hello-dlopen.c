#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>

typedef int (*hellofn)(void);

int main(void) 
{
    int retval = 1;

    /* have a buffer bigger than a page, so we can maybe find out
       if we put the data segment in the wrong place. */
    int i;
    static char buffer[128 * 1024];
    for (i = 0; i < sizeof (buffer); i++)
        buffer[i] = (char) i;


    void *lib = dlopen("./hello.so", RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL)
        perror("dlopen");
    else
    {
        hellofn phello = (hellofn) dlsym(lib, "hello");
        if (phello == NULL)
            perror("dlsym");
        else
        {
            retval = phello();
            dlclose(lib);
        }
    }

    return retval;
}

/* end of hello-dlopen.c ... */

