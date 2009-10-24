/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/* code shared between all FatELF utilities... */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fatelf.h"

#if !FATELF_UTILS
#error Do not include this file outside of FatELF.
#endif

#ifdef __GNUC__
#define FATELF_ISPRINTF(x,y) __attribute__((format (printf, x, y)))
#else
#define FATELF_ISPRINTF(x,y)
#endif

extern const char *unlink_on_xfail;
extern const char *fatelf_build_version;

#define FATELF_WANT_MACHINE   (1 << 0)
#define FATELF_WANT_OSABI     (1 << 1)
#define FATELF_WANT_OSABIVER  (1 << 2)
#define FATELF_WANT_WORDSIZE  (1 << 3)
#define FATELF_WANT_BYTEORDER (1 << 4)
#define FATELF_WANT_EVERYTHING 0xFFFF

typedef struct fatelf_machine_info
{
    uint16_t id;
    const char *name;
    const char *desc;
} fatelf_machine_info;


typedef struct fatelf_osabi_info
{
    uint8_t id;
    const char *name;
    const char *desc;
} fatelf_osabi_info;


// all functions that start with 'x' may call exit() on error!

// Report an error to stderr and terminate immediately with exit(1).
void xfail(const char *fmt, ...) FATELF_ISPRINTF(1,2);

// Wrap malloc() with an xfail(), so this returns memory or calls exit().
// Memory is guaranteed to be initialized to zero.
void *xmalloc(const size_t len);

// Allocate a copy of (str), xfail() on allocation failure.
char *xstrdup(const char *str);

// These all xfail() on error and handle EINTR for you.
int xopen(const char *fname, const int flags, const int perms);
ssize_t xread(const char *fname, const int fd, void *buf,
              const size_t len, const int must_read);
ssize_t xwrite(const char *fname, const int fd,
               const void *buf, const size_t len);
void xclose(const char *fname, const int fd);
void xlseek(const char *fname, const int fd, const off_t o, const int whence);

// This writes len null bytes to (fd).
void xwrite_zeros(const char *fname, const int fd, size_t len);

// copy file from infd to current seek position in outfd, until infd's EOF.
uint64_t xcopyfile(const char *in, const int infd,
                   const char *out, const int outfd);

// copy file from infd to current offset in outfd, for size bytes.
void xcopyfile_range(const char *in, const int infd,
                     const char *out, const int outfd,
                     const uint64_t offset, const uint64_t size);

// get the length of an open file in bytes.
uint64_t xget_file_size(const char *fname, const int fd);

// read the parts of an ELF header we care about.
void xread_elf_header(const char *fname, const int fd, const uint64_t offset,
                      FATELF_record *rec);

// How many bytes to allocate for a FATELF_header.
size_t fatelf_header_size(const int bincount);

// Put FatELF header to disk. Will seek to 0 first.
void xwrite_fatelf_header(const char *fname, const int fd,
                          const FATELF_header *header);

// Get FatELF header from disk. Will seek to 0 first.
// don't forget to free() the returned pointer!
FATELF_header *xread_fatelf_header(const char *fname, const int fd);

// Locate non-FatELF data at the end of a FatELF file fd, based on
//  header header. Returns non-zero if junk found, and fills in offset and
//  size.
int xfind_junk(const char *fname, const int fd, const FATELF_header *header,
               uint64_t *offset, uint64_t *size);

// Write non-FatELF data at the end of FatELF file fd to current position in
//  outfd, based on header header.
void xappend_junk(const char *fname, const int fd,
                  const char *out, const int outfd,
                  const FATELF_header *header);

// Align a value to the page size.
uint64_t align_to_page(const uint64_t offset);

// find the record closest to the end of the file. -1 on error!
int find_furthest_record(const FATELF_header *header);

const fatelf_machine_info *get_machine_by_id(const uint16_t id);
const fatelf_machine_info *get_machine_by_name(const char *name);
const fatelf_osabi_info *get_osabi_by_id(const uint8_t id);
const fatelf_osabi_info *get_osabi_by_name(const char *name);

// Returns a string that can be used to target a specific record.
const char *fatelf_get_target_name(const FATELF_record *rec, const int wants);

// these return static strings of english words.
const char *fatelf_get_wordsize_string(const uint8_t wordsize);
const char *fatelf_get_byteorder_name(const uint8_t byteorder);
const char *fatelf_get_byteorder_target_name(const uint8_t byteorder);
const char *fatelf_get_wordsize_target_name(const uint8_t wordsize);

// Find the desired record in the FatELF header, based on a string in
//  various formats.
int xfind_fatelf_record(const FATELF_header *header, const char *target);

// non-zero if all pertinent fields in a match b.
int fatelf_record_matches(const FATELF_record *a, const FATELF_record *b);

// Call this at the start of main().
void xfatelf_init(int argc, const char **argv);

// end of fatelf-utils.h ...

