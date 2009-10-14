int hello(void);
int main(void)
{
    /* have a buffer bigger than a page, so we can maybe find out
       if we put the data segment in the wrong place. */
    int i;
    static char buffer[128 * 1024];
    for (i = 0; i < sizeof (buffer); i++)
        buffer[i] = (char) i;

    return hello();
}
