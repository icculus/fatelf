/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

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

static const char *unlink_on_fail = NULL;
static uint8_t zerobuf[4096];

// Report an error to stderr and terminate immediately with exit(1).
static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (unlink_on_fail != NULL)
        unlink(unlink_on_fail);  // don't care if this fails.
    unlink_on_fail = NULL;

    exit(1);
} // fail


// Wrap malloc() with a fail(), so this returns memory or calls exit().
// Memory is guaranteed to be initialized to zero.
static void *xmalloc(const size_t len)
{
    void *retval = calloc(1, len);
    if (retval == NULL)
        fail("Out of memory!");
    return retval;
} // xmalloc


// fail() on error.
static int xopen(const char *fname, const int flags, const int perms)
{
    const int retval = open(fname, flags, perms);
    if (retval == -1)
        fail("Failed to open '%s': %s", fname, strerror(errno));
    return retval;
} // xopen


// fail() on error, handle EINTR.
static ssize_t xread(const char *fname, const int fd, void *buf,
                     const size_t len, const int must_read)
{
    ssize_t rc;
    while (((rc = read(fd,buf,len)) == -1) && (errno == EINTR)) { /* spin */ }
    if ( (rc == -1) || ((must_read) && (rc != len)) )
        fail("Failed to read '%s': %s", fname, strerror(errno));
    return rc;
} // xread


// fail() on error, handle EINTR.
static ssize_t xwrite(const char *fname, const int fd,
                      const void *buf, const size_t len)
{
    ssize_t rc;
    while (((rc = write(fd,buf,len)) == -1) && (errno == EINTR)) { /* spin */ }
    if (rc == -1)
        fail("Failed to write '%s': %s", fname, strerror(errno));
    return rc;
} // xwrite

// fail() on error, handle EINTR.
static void xwrite_zeros(const char *fname, const int fd, size_t len)
{
    while (len > 0)
    {
        const size_t count = (len < sizeof (zerobuf)) ? len : sizeof (zerobuf);
        xwrite(fname, fd, zerobuf, count);
        len -= count;
    } // while
} // xwrite_zeros

// fail() on error, handle EINTR.
static void xclose(const char *fname, const int fd)
{
    int rc;
    while ( ((rc = close(fd)) == -1) && (errno == EINTR) ) { /* spin. */ }
    if (rc == -1)
        fail("Failed to close '%s': %s", fname, strerror(errno));
} // xopen


// fail() on error.
static void xlseek(const char *fname, const int fd,
                   const off_t offset, const int whence)
{
    if (lseek(fd, offset, whence) == -1)
        fail("Failed to seek in '%s': %s", fname, strerror(errno));
} // xlseek


// fail() on error.
static uint64_t xcopyfile(const char *in, const int infd,
                      const char *out, const int outfd)
{
    // !!! FIXME: use sendfile() on Linux.
    static uint8_t buf[256 * 1024];
    uint64_t retval = 0;
    ssize_t rc = 0;
    xlseek(in, infd, 0, SEEK_SET);
    while ( (rc = xread(in, infd, buf, sizeof (buf), 0)) > 0 )
    {
        xwrite(out, outfd, buf, rc);
        retval += (uint64_t) rc;
    } // while

    return retval;
} // xcopyfile


static void xread_elf_header(const char *fname, const int fd,
                             FATELF_binary_info *info)
{
    const uint8_t magic[4] = { 0x7F, 0x45, 0x4C, 0x46 };
    uint8_t buf[20];  // we only care about the first 20 bytes.
    xlseek(fname, fd, 0, SEEK_SET);  // just in case.
    xread(fname, fd, buf, sizeof (buf), 1);
    if (memcmp(magic, buf, sizeof (magic)) != 0)
        fail("'%s' is not an ELF binary");
    info->abi = (uint16_t) buf[7];
    info->abi_version = (uint16_t) buf[8];
    if (buf[5] == 0)  // bigendian
        info->cpu = (((uint32_t)buf[18]) << 8) | (((uint32_t)buf[19]));
    else if (buf[5] == 1)  // littleendian
        info->cpu = (((uint32_t)buf[19]) << 8) | (((uint32_t)buf[18]));
    else
        fail("Unexpected data encoding in '%s'", fname);
} // xread_elf_header


// Write a native uint16_t to a buffer in littleendian format.
static inline uint8_t *putui16(uint8_t *ptr, const uint16_t val)
{
    *(ptr++) = ((uint8_t) ((val >> 0) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 8) & 0xFF));
    return ptr;
} // putui16


// Write a native uint32_t to a buffer in littleendian format.
static inline uint8_t *putui32(uint8_t *ptr, const uint32_t val)
{
    *(ptr++) = ((uint8_t) ((val >> 0) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 8) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 16) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 24) & 0xFF));
    return ptr;
} // putui32


// Write a native uint64_t to a buffer in littleendian format.
static inline uint8_t *putui64(uint8_t *ptr, const uint64_t val)
{
    *(ptr++) = ((uint8_t) ((val >> 0) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 8) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 16) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 24) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 32) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 40) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 48) & 0xFF));
    *(ptr++) = ((uint8_t) ((val >> 56) & 0xFF));
    return ptr;
} // putui64


static void xwrite_fatelf_header(const char *fname, const int fd,
                                 const FATELF_header *header)
{
    const size_t buflen = FATELF_DISK_FORMAT_SIZE(header->num_binaries);
    uint8_t *buf = (uint8_t *) xmalloc(buflen);
    uint8_t *ptr = buf;
    int i;

    ptr = putui32(ptr, header->magic);
    ptr = putui16(ptr, header->version);
    ptr = putui16(ptr, header->num_binaries);

    for (i = 0; i < header->num_binaries; i++)
    {
        ptr = putui16(ptr, header->binaries[i].abi);
        ptr = putui16(ptr, header->binaries[i].abi_version);
        ptr = putui32(ptr, header->binaries[i].cpu);
        ptr = putui64(ptr, header->binaries[i].offset);
    } // for

    assert(ptr == (buf + buflen));

    xlseek(fname, fd, 0, SEEK_SET);  // jump to start of file again.
    xwrite(fname, fd, buf, buflen);

    free(buf);
} // xwrite_fatelf_header


// Return enough bytes to definitely cover both memory and on-disk format.
static inline size_t header_size(const int bincount)
{
    return (sizeof (FATELF_header) + (sizeof (FATELF_binary_info) * bincount));
} // header_size

static uint64_t align_to_page(const uint64_t offset)
{
    const size_t pagesize = 4096;  // !!! FIXME: hardcoded pagesize.
    const size_t overflow = (offset % pagesize);
    return overflow ? (offset + (pagesize - overflow)) : offset;
} // align_to_page

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

