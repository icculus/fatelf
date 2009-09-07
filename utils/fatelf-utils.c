/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/* code shared between all FatELF utilities... */

#define FATELF_UTILS 1
#include "fatelf-utils.h"

const char *unlink_on_xfail = NULL;
static uint8_t zerobuf[4096];

// Report an error to stderr and terminate immediately with exit(1).
void xfail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (unlink_on_xfail != NULL)
        unlink(unlink_on_xfail);  // don't care if this fails.
    unlink_on_xfail = NULL;

    exit(1);
} // xfail


// Wrap malloc() with an xfail(), so this returns memory or calls exit().
// Memory is guaranteed to be initialized to zero.
void *xmalloc(const size_t len)
{
    void *retval = calloc(1, len);
    if (retval == NULL)
        xfail("Out of memory!");
    return retval;
} // xmalloc


// xfail() on error.
int xopen(const char *fname, const int flags, const int perms)
{
    const int retval = open(fname, flags, perms);
    if (retval == -1)
        xfail("Failed to open '%s': %s", fname, strerror(errno));
    return retval;
} // xopen


// xfail() on error, handle EINTR.
ssize_t xread(const char *fname, const int fd, void *buf,
              const size_t len, const int must_read)
{
    ssize_t rc;
    while (((rc = read(fd,buf,len)) == -1) && (errno == EINTR)) { /* spin */ }
    if ( (rc == -1) || ((must_read) && (rc != len)) )
        xfail("Failed to read '%s': %s", fname, strerror(errno));
    return rc;
} // xread


// xfail() on error, handle EINTR.
ssize_t xwrite(const char *fname, const int fd,
               const void *buf, const size_t len)
{
    ssize_t rc;
    while (((rc = write(fd,buf,len)) == -1) && (errno == EINTR)) { /* spin */ }
    if (rc == -1)
        xfail("Failed to write '%s': %s", fname, strerror(errno));
    return rc;
} // xwrite

// xfail() on error, handle EINTR.
void xwrite_zeros(const char *fname, const int fd, size_t len)
{
    while (len > 0)
    {
        const size_t count = (len < sizeof (zerobuf)) ? len : sizeof (zerobuf);
        xwrite(fname, fd, zerobuf, count);
        len -= count;
    } // while
} // xwrite_zeros

// xfail() on error, handle EINTR.
void xclose(const char *fname, const int fd)
{
    int rc;
    while ( ((rc = close(fd)) == -1) && (errno == EINTR) ) { /* spin. */ }
    if (rc == -1)
        xfail("Failed to close '%s': %s", fname, strerror(errno));
} // xopen


// xfail() on error.
void xlseek(const char *fname, const int fd,
            const off_t offset, const int whence)
{
    if (lseek(fd, offset, whence) == -1)
        xfail("Failed to seek in '%s': %s", fname, strerror(errno));
} // xlseek


// xfail() on error.
uint64_t xcopyfile(const char *in, const int infd,
                   const char *out, const int outfd)
{
    // !!! FIXME: use sendfile() on Linux (if it'll handle non-socket fd's).
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


void xread_elf_header(const char *fname, const int fd, FATELF_binary_info *info)
{
    const uint8_t magic[4] = { 0x7F, 0x45, 0x4C, 0x46 };
    uint8_t buf[20];  // we only care about the first 20 bytes.
    xlseek(fname, fd, 0, SEEK_SET);  // just in case.
    xread(fname, fd, buf, sizeof (buf), 1);
    if (memcmp(magic, buf, sizeof (magic)) != 0)
        xfail("'%s' is not an ELF binary");
    info->abi = (uint16_t) buf[7];
    info->abi_version = (uint16_t) buf[8];
    if (buf[5] == 0)  // bigendian
        info->cpu = (((uint32_t)buf[18]) << 8) | (((uint32_t)buf[19]));
    else if (buf[5] == 1)  // littleendian
        info->cpu = (((uint32_t)buf[19]) << 8) | (((uint32_t)buf[18]));
    else
        xfail("Unexpected data encoding in '%s'", fname);
} // xread_elf_header


size_t fatelf_header_size(const int bincount)
{
    return (sizeof (FATELF_header) + (sizeof (FATELF_binary_info) * bincount));
} // fatelf_header_size


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


// Read a littleendian uint16_t from a buffer in native format.
static inline uint8_t *getui16(uint8_t *ptr, uint16_t *val)
{
    *val = ( (((uint16_t) ptr[0]) << 0) | (((uint16_t) ptr[1]) << 8) );
    return ptr + sizeof (*val);
} // getui16


// Read a littleendian uint32_t from a buffer in native format.
static inline uint8_t *getui32(uint8_t *ptr, uint32_t *val)
{
    *val = ( (((uint32_t) ptr[0]) << 0)  |
             (((uint32_t) ptr[1]) << 8)  |
             (((uint32_t) ptr[2]) << 16) |
             (((uint32_t) ptr[3]) << 24) );
    return ptr + sizeof (*val);
} // getui32


// Read a littleendian uint64_t from a buffer in native format.
static inline uint8_t *getui64(uint8_t *ptr, uint64_t *val)
{
    *val = ( (((uint64_t) ptr[0]) << 0)  |
             (((uint64_t) ptr[1]) << 8)  |
             (((uint64_t) ptr[2]) << 16) |
             (((uint64_t) ptr[3]) << 24) |
             (((uint64_t) ptr[4]) << 32) |
             (((uint64_t) ptr[5]) << 40) |
             (((uint64_t) ptr[6]) << 48) |
             (((uint64_t) ptr[7]) << 56) );
    return ptr + sizeof (*val);
} // getui64


void xwrite_fatelf_header(const char *fname, const int fd,
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

// don't forget to free() the returned pointer!
FATELF_header *xread_fatelf_header(const char *fname, const int fd)
{
    FATELF_header *header = NULL;
    uint8_t buf[8];
    uint8_t *fullbuf = NULL;
    uint8_t *ptr = buf;
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t bincount = 0;
    size_t buflen = 0;
    int i = 0;

    xlseek(fname, fd, 0, SEEK_SET);  // just in case.
    xread(fname, fd, buf, sizeof (buf), 1);
    ptr = getui32(ptr, &magic);
    ptr = getui16(ptr, &version);
    ptr = getui16(ptr, &bincount);

    if (magic != FATELF_MAGIC)
        xfail("'%s' is not a FatELF binary.", fname);
    else if (version != 1)
        xfail("'%s' uses an unknown FatELF version.", fname);
    
    buflen = FATELF_DISK_FORMAT_SIZE(bincount) - sizeof (buf);
    ptr = fullbuf = (uint8_t *) xmalloc(buflen);
    xread(fname, fd, fullbuf, buflen, 1);

    header = (FATELF_header *) xmalloc(fatelf_header_size(bincount));
    header->magic = magic;
    header->version = version;
    header->num_binaries = bincount;

    for (i = 0; i < bincount; i++)
    {
        ptr = getui16(ptr, &header->binaries[i].abi);
        ptr = getui16(ptr, &header->binaries[i].abi_version);
        ptr = getui32(ptr, &header->binaries[i].cpu);
        ptr = getui64(ptr, &header->binaries[i].offset);
    } // for

    assert(ptr == (fullbuf + buflen));

    free(fullbuf);
    return header;
} // xread_fatelf_header


uint64_t align_to_page(const uint64_t offset)
{
    const size_t pagesize = 4096;  // !!! FIXME: hardcoded pagesize.
    const size_t overflow = (offset % pagesize);
    return overflow ? (offset + (pagesize - overflow)) : offset;
} // align_to_page


const char *abi_name_string(const uint16_t abitype)
{
    return "???";  // !!! FIXME
} // abi_name_string


const char *cpu_name_string(const uint32_t cputype)
{
    return "???";  // !!! FIXME
} // cpu_name_string


void xfatelf_init(int argc, const char **argv)
{
    memset(zerobuf, '\0', sizeof (zerobuf));  // just in case.
} // xfatelf_init

// end of fatelf-utils.c ...

