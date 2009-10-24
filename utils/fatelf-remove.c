/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int fatelf_remove(const char *out, const char *fname,
                         const char *target)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    const int idx = xfind_fatelf_record(header, target);
    const int outfd = xopen(out, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    uint64_t offset = FATELF_DISK_FORMAT_SIZE(((int)header->num_records));
    int i;

    unlink_on_xfail = out;

    // pad out some bytes for the header we'll write at the end...
    xwrite_zeros(out, outfd, (size_t) offset);

    for (i = 0; i < ((int) header->num_records); i++)
    {
        if (i != idx)  // not the thing we're removing?
        {
            const uint64_t binary_offset = align_to_page(offset);
            FATELF_record *rec = &header->records[i];

            // append this binary to the final file, padded to page alignment.
            xwrite_zeros(out, outfd, (size_t) (binary_offset - offset));
            xcopyfile_range(fname, fd, out, outfd, binary_offset, rec->size);

            rec->offset = binary_offset;
            offset = binary_offset + rec->size;
        } // if
    } // for

    // remove the record we chopped out.
    header->num_records--;
    if (idx < ((int) header->num_records))
    {
        void *dst = &header->records[idx];
        const void *src = &header->records[idx+1];
        const size_t count = (header->num_records - idx);
        memmove(dst, src, sizeof (FATELF_record) * count);
    } // if

    xappend_junk(fname, fd, out, outfd, header);

    // Write the actual FatELF header now...
    xwrite_fatelf_header(out, outfd, header);

    xclose(out, outfd);
    xclose(fname, fd);
    free(header);

    unlink_on_xfail = NULL;

    return 0;  // success.
} // fatelf_remove


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 4)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <out> <in> <target>", argv[0]);
    return fatelf_remove(argv[1], argv[2], argv[3]);
} // main

// end of fatelf-remove.c ...

