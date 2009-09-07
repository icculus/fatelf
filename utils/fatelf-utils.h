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
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>

#include "fatelf.h"

#if !FATELF_UTILS
#error Do not include this file outside of FatELF.
#endif

extern const char *unlink_on_xfail;


typedef struct fatelf_machine_info
{
    uint32_t id;
    const char *name;
    const char *desc;
} fatelf_machine_info;


typedef struct fatelf_abi_info
{
    uint16_t id;
    const char *name;
    const char *desc;
} fatelf_abi_info;


// all functions that start with 'x' may call exit() on error!

// Report an error to stderr and terminate immediately with exit(1).
void xfail(const char *fmt, ...);

// Wrap malloc() with an xfail(), so this returns memory or calls exit().
// Memory is guaranteed to be initialized to zero.
void *xmalloc(const size_t len);

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

// read the parts of an ELF header we care about.
void xread_elf_header(const char *fname, const int fd, FATELF_binary_info *i);

// How many bytes to allocate for a FATELF_header.
size_t fatelf_header_size(const int bincount);

// Put FatELF header to disk. Will seek to 0 first.
void xwrite_fatelf_header(const char *fname, const int fd,
                          const FATELF_header *header);

// Get FatELF header from disk. Will seek to 0 first.
// don't forget to free() the returned pointer!
FATELF_header *xread_fatelf_header(const char *fname, const int fd);

// Align a value to the page size.
uint64_t align_to_page(const uint64_t offset);

const fatelf_machine_info *get_machine_by_id(const uint32_t id);
const fatelf_machine_info *get_machine_by_name(const char *name);
const fatelf_abi_info *get_abi_by_id(const uint16_t id);
const fatelf_abi_info *get_abi_by_name(const char *name);

// Call this at the start of main().
void xfatelf_init(int argc, const char **argv);

// end of fatelf-utils.h ...

