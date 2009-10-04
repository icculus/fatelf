#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void check_elf(const char *fname)
{
	static const unsigned char magic[4] = { 0x7F, 0x45, 0x4C, 0x46 };
	size_t br = 0;
	unsigned char buf[4];
	FILE *f = fopen(fname, "rb");
	if (f == NULL)
	{
		fprintf(stderr, "Can't open %s: %s\n", fname, strerror(errno));
		return;
	}

	br = fread(buf, 4, 1, f);
	fclose(f);

	if (br != 1)
	{
		if (ferror(f))
			fprintf(stderr, "Can't read %s: %s\n", fname, strerror(errno));
		return;
	}

	if (memcmp(buf, magic, 4) == 0)
		printf("%s\n", fname);
}

int main(int argc, const char **argv)
{
	int i;
	for (i = 1; i < argc; i++)
		check_elf(argv[i]);
	return 0;
}

// end of iself.c ...


