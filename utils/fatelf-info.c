/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static int fatelf_info(const char *fname)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    unsigned int i = 0;
    uint64_t junkoffset, junksize;

    printf("%s: FatELF format version %d\n", fname, (int) header->version);
    printf("%d records.\n", (int) header->num_records);

    if (xfind_junk(fname, fd, header, &junkoffset, &junksize))
    {
        printf("%llu bytes of junk appended, starting at offset %llu.\n",
               (unsigned long long) junksize, (unsigned long long) junkoffset);
    } // if

    for (i = 0; i < header->num_records; i++)
    {
        const FATELF_record *rec = &header->records[i];
        const fatelf_machine_info *machine = get_machine_by_id(rec->machine);
        const fatelf_osabi_info *osabi = get_osabi_by_id(rec->osabi);

        printf("Binary at index #%d:\n", i);
        printf("  OSABI %u (%s%s%s) version %u,\n",
                (unsigned int) rec->osabi, osabi ? osabi->name : "???",
                osabi ? ": " : "", osabi ? osabi->desc : "",
                (unsigned int) rec->osabi_version);
        printf("  %s bits\n", fatelf_get_wordsize_string(rec->word_size));
        printf("  %s byteorder\n", fatelf_get_byteorder_name(rec->byte_order));
        printf("  Machine %u (%s%s%s)\n",
                (unsigned int) rec->machine, machine ? machine->name : "???",
                machine ? ": " : "", machine ? machine->desc : "");
        printf("  Offset %llu\n", (unsigned long long) rec->offset);
        printf("  Size %llu\n", (unsigned long long) rec->size);
        printf("  Target name: '%s' or 'record%u'\n",
               fatelf_get_target_name(rec, FATELF_WANT_EVERYTHING), i);
    } // for

    xclose(fname, fd);
    free(header);

    return 0;  // success.
} // fatelf_info


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 2)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <fname>", argv[0]);
    return fatelf_info(argv[1]);
} // main

// end of fatelf-info.c ...

