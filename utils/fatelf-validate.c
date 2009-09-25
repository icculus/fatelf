/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int fatelf_validate(const char *fname)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    int i;

    if (header->reserved0 != 0)
        xfail("FatELF header reserved field isn't zero.");

    for (i = 0; i < ((int)header->num_records); i++)
    {
        const FATELF_record *rec = &header->records[i];
        FATELF_record elfrec;

        if (rec->reserved0 != 0)
            xfail("Reserved0 field is not zero in record #%d", i);
        else if (rec->reserved1 != 0)
            xfail("Reserved1 field is not zero in record #%d", i);
        else if (!get_machine_by_id(rec->machine))
            xfail("Unknown machine #%d in record #%d", (int) rec->machine, i);
        else if (!get_osabi_by_id(rec->osabi))
            xfail("Unknown OSABI #%d in record #%d", (int) rec->osabi, i);
        else if (!fatelf_get_byteorder_target_name(rec->byte_order))
            xfail("Unknown byte order #%d in record #%d", (int) rec->byte_order, i);
        else if (!fatelf_get_wordsize_target_name(rec->word_size))
            xfail("Unknown word size #%d in record #%d", (int) rec->word_size, i);
        else if (rec->offset != align_to_page(rec->offset))
            xfail("Unaligned binary in record #%d", i);
        else if ((rec->offset + rec->size) < rec->offset)
        {
            xfail("Bogus offset+size (%llu + %llu) in record #%d",
                  (unsigned long long) rec->offset,
                  (unsigned long long) rec->size, i);
        } // else if
        else if ( (rec->word_size == FATELF_32BITS) &&
                  ((rec->offset + rec->size) > 0xFFFFFFFF) )
        {
            xfail("32-bit binary past 4 gig limit in record #%d", i);
        } // else if

        // !!! FIXME: check for overlap between records?

        xread_elf_header(fname, fd, rec->offset, &elfrec);
        if (!fatelf_record_matches(rec, &elfrec))
            xfail("ELF header differs from FatELF data in record #%d", i);
    } // for

    xclose(fname, fd);
    free(header);
    return 0;  // success
} // fatelf_validate


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 2)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <in>", argv[0]);
    return fatelf_validate(argv[1]);
} // main

// end of fatelf-validate.c ...

