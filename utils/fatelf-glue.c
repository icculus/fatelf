/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

// !!! FIXME: this include is nasty.
#define FATELF_UTILS 1
#include "fatelf-utils.c"

static int fatelf_glue(const char *out, const char **bins, const int bincount)
{
    int i = 0;
    const size_t struct_size = header_size(bincount);
    FATELF_header *header = (FATELF_header *) xmalloc(struct_size);
    const int outfd = xopen(out, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    uint64_t offset = FATELF_DISK_FORMAT_SIZE(bincount);

    unlink_on_fail = out;

    if (bincount == 0)
        fail("Nothing to do.");
    else if (bincount > 0xFFFF)
        fail("Too many binaries (max is %d).", 0xFFFF);

    header->magic = FATELF_MAGIC;
    header->version = FATELF_FORMAT_VERSION;
    header->num_binaries = bincount;

    for (i = 0; i < bincount; i++)
    {
        int j = 0;
        const uint64_t binary_offset = align_to_page(offset);
        const char *fname = bins[i];
        const int fd = xopen(fname, O_RDONLY, 0755);
        FATELF_binary_info *info = &header->binaries[i];

        info->offset = binary_offset;
        xread_elf_header(fname, fd, info);

        // make sure we don't have a duplicate target.
        for (j = 0; j < i; j++)
        {
            const FATELF_binary_info *other = &header->binaries[j];
            const int same = (other->cpu == info->cpu) &&
                             (other->abi == info->abi) &&
                             (other->abi_version == info->abi_version);
            if (same)
                fail("'%s' and '%s' are for the same target.", bins[j], fname);
        } // for

        // append this binary to the final file, padded to page alignment.
        xwrite_zeros(out, outfd, (size_t) (binary_offset - offset));
        offset += binary_offset + xcopyfile(fname, fd, out, outfd);

        // done with this binary!
        xclose(fname, fd);
    } // for

    // Write the actual FatELF header now...
    xwrite_fatelf_header(out, outfd, header);
    xclose(out, outfd);
    free(header);

    unlink_on_fail = NULL;

    return 0;  // success.
} // fatelf_glue


int main(int argc, const char **argv)
{
    memset(zerobuf, '\0', sizeof (zerobuf));  // just in case.
    if (argc < 4)  // this could stand to use getopt(), later.
        fail("USAGE: %s <out> <bin1> <bin2> [... binN]", argv[0]);
    return fatelf_glue(argv[1], &argv[2], argc - 2);
} // main

// end of fatelf-glue.c ...

