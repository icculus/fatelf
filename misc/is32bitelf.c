#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static int check_elf(const char *fname)
{
	static const unsigned char magic[4] = { 0x7F, 0x45, 0x4C, 0x46 };
	size_t br = 0;
	unsigned char buf[5];
	FILE *f = fopen(fname, "rb");
	if (f == NULL)
	{
		fprintf(stderr, "Can't open %s: %s\n", fname, strerror(errno));
		return 1;
	}

	br = fread(buf, 5, 1, f);
	fclose(f);

	if (br != 1)
	{
		if (ferror(f))
			fprintf(stderr, "Can't read %s: %s\n", fname, strerror(errno));
		return 1;
	}

	if (memcmp(buf, magic, 4) != 0)
	{
		fprintf(stderr, "Not an ELF file\n");
		return 1;
	}
	return buf[4] == 1 ? 0 : 1;
}

int main(int argc, const char **argv)
{
	return check_elf(argv[1]);
}

// end of iself.c ...


