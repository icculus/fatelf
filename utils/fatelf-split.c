/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

static char *make_filename(const char *base, const int wants,
                           const FATELF_record *rec)
{
    const char *target = fatelf_get_target_name(rec, wants);
    const size_t len = strlen(base) + strlen(target) + 2;
    char *retval = (char *) xmalloc(len);
    snprintf(retval, len, "%s-%s", base, target);
    return retval;
} // make_filename


static inline int unsorted(const FATELF_record *a, const FATELF_record *b)
{
    #define TEST_UNSORTED(field) \
        if (a->field > b->field) \
            return 1; \
        else if (a->field < b->field) \
            return 0;

    // This is the in order of precedence of fields.
    TEST_UNSORTED(machine);
    TEST_UNSORTED(word_size);
    TEST_UNSORTED(byte_order);
    TEST_UNSORTED(osabi);
    TEST_UNSORTED(osabi_version);

    #undef TEST_UNSORTED

    return 0;
} // unsorted


static int fatelf_split(const char *fname)
{
    const int fd = xopen(fname, O_RDONLY, 0755);
    FATELF_header *header = xread_fatelf_header(fname, fd);
    const size_t len = sizeof (FATELF_record *) * header->num_records;
    FATELF_record **sorted = (FATELF_record **) xmalloc(len);
    const int maxrecs = header->num_records;
    int is_sorted = 1;
    int i = 0;

    // Try to keep the filenames as short as possible. To start, sort
    //  the records so we know which items are relevant.
    for (i = 0; i < ((int) header->num_records); i++)
        sorted[i] = &header->records[i];

    do  // bubble sort ftw.
    {
        is_sorted = 1;
        for (i = 0; i < ((int)header->num_records)-1; i++)
        {
            if (unsorted(sorted[i], sorted[i+1]))
            {
                FATELF_record *tmp = sorted[i];
                sorted[i] = sorted[i+1];
                sorted[i+1] = tmp;
                is_sorted = 0;
            } // if
        } // for
    } while (!is_sorted);

    // now dump each ELF file, naming it with just the minimum set of
    //  attributes that make it unique. We do this by checking the item
    //  sorted before it and after it to see what matches.

    // This isn't perfect...you might get a list like this case...
    //
    //   mybin-i386
    //   mybin-ppc:32bits:be
    //   mybin-ppc:32bits:le
    //   mybin-ppc64
    //   mybin-x86_64
    //
    // ...where those "32bits:" parts are superfluous.

    for (i = 0; i < maxrecs; i++)
    {
        const FATELF_record *rec = sorted[i];
        const FATELF_record *prev = (i > 0) ? sorted[i-1] : NULL;
        const FATELF_record *next = (i < maxrecs-1) ? sorted[i+1] : NULL;
        char *out = NULL;
        int outfd = -1;
        int wants = 0;
        int unique = 0;

        #define TEST_WANT(field, flag) \
            if (!unique) { \
                wants |= FATELF_WANT_##flag; \
                if ((prev) && (prev->field != rec->field)) prev = NULL; \
                if ((next) && (next->field != rec->field)) next = NULL; \
                if (!prev && !next) unique = 1; \
            }

        // This must be in the same order as the TEST_UNSORTED things above.
        TEST_WANT(machine, MACHINE);
        TEST_WANT(word_size, WORDSIZE);
        TEST_WANT(byte_order, BYTEORDER);
        TEST_WANT(osabi, OSABI);
        TEST_WANT(osabi_version, OSABIVER);

        #undef TEST_WANT

        out = make_filename(fname, wants, rec);
        outfd = xopen(out, O_WRONLY | O_CREAT | O_TRUNC, 0755);
        unlink_on_xfail = out;
        xcopyfile_range(fname, fd, out, outfd, rec->offset, rec->size);
        xappend_junk(fname, fd, out, outfd, header);
        xclose(out, outfd);
        unlink_on_xfail = NULL;
        free(out);
    } // for

    free(sorted);
    xclose(fname, fd);
    free(header);

    return 0;  // success.
} // fatelf_split


int main(int argc, const char **argv)
{
    xfatelf_init(argc, argv);
    if (argc != 2)  // this could stand to use getopt(), later.
        xfail("USAGE: %s <in>", argv[0]);
    return fatelf_split(argv[1]);
} // main

// end of fatelf-split.c ...

