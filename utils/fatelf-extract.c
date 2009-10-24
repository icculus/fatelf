/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int fatelf_extract(const char *out, const char *fname, 
                          const char *target)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    const int recidx = xfind_fatelf_record(header, target);
    const int outfd = xopen(out, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    const FATELF_record *rec = &header->records[recidx];

    unlink_on_xfail = out;

    xcopyfile_range(fname, fd, out, outfd, rec->offset, rec->size);
    xappend_junk(fname, fd, out, outfd, header);
    xclose(out, outfd);
    xclose(fname, fd);
    free(header);

    unlink_on_xfail = NULL;

    return 0;  // success.
} // fatelf_extract


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 4)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <out> <in> <target>", argv[0]);
    return fatelf_extract(argv[1], argv[2], argv[3]);
} // main

// end of fatelf-extract.c ...

