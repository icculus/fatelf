/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int fatelf_verify(const char *fname, const char *target)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    const int recidx = xfind_fatelf_record(header, target);
    xclose(fname, fd);
    free(header);
    return (recidx < 0);
} // fatelf_verify


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 3)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <in> <target>", argv[0]);
    return fatelf_verify(argv[1], argv[2]);
} // main

// end of fatelf-verify.c ...

