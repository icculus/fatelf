/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int xfind_fatelf_record_by_elf(const char *fname, const int fd,
                                      const char *fatfname,
                                      const FATELF_header *header)
{
    FATELF_record record;
    int i;

    xread_elf_header(fname, fd, 0, &record);
    for (i = 0; i < ((int)header->num_records); i++)
    {
        if (fatelf_record_matches(&header->records[i], &record))
            return i;
    } // for

    xfail("No record matches '%s' in FatELF file '%s'", fname, fatfname);
    return -1;
} // xfind_fatelf_record_by_elf


static int fatelf_replace(const char *out, const char *fname,
                          const char *newobj)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    const int newfd = xopen(newobj, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    const int idx = xfind_fatelf_record_by_elf(newobj, newfd, fname, header);
    const int outfd = xopen(out, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    uint64_t offset = FATELF_DISK_FORMAT_SIZE(((int)header->num_records));
    int i;

    unlink_on_xfail = out;

    // pad out some bytes for the header we'll write at the end...
    xwrite_zeros(out, outfd, (size_t) offset);

    for (i = 0; i < ((int) header->num_records); i++)
    {
        const uint64_t binary_offset = align_to_page(offset);
        FATELF_record *rec = &header->records[i];

        // append this binary to the final file, padded to page alignment.
        xwrite_zeros(out, outfd, (size_t) (binary_offset - offset));

        if (i == idx)  // the thing we're replacing...
            rec->size = xcopyfile(newobj, newfd, out, outfd);
        else
            xcopyfile_range(fname, fd, out, outfd, binary_offset, rec->size);

        rec->offset = binary_offset;
        offset = binary_offset + rec->size;
    } // for

    xappend_junk(fname, fd, out, outfd, header);

    // Write the actual FatELF header now...
    xwrite_fatelf_header(out, outfd, header);

    xclose(out, outfd);
    xclose(newobj, newfd);
    xclose(fname, fd);
    free(header);

    unlink_on_xfail = NULL;

    return 0;  // success.
} // fatelf_replace


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 4)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <out> <in> <newelf>", argv[0]);
    return fatelf_replace(argv[1], argv[2], argv[3]);
} // main

// end of fatelf-replace.c ...

