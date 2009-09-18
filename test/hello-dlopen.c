#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>

typedef int (*hellofn)(void);

int main(void) 
{
    int retval = 1;
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

