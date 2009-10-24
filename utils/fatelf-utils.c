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

#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

const char *unlink_on_xfail = NULL;
static uint8_t zerobuf[4096];


#ifndef APPID
#define APPID fatelf
#endif

#ifndef APPREV
#define APPREV "???"
#endif

#if (defined __GNUC__)
#   define VERSTR2(x) #x
#   define VERSTR(x) VERSTR2(x)
#   define COMPILERVER " " VERSTR(__GNUC__) "." VERSTR(__GNUC_MINOR__) "." VERSTR(__GNUC_PATCHLEVEL__)
#elif (defined __SUNPRO_C)
#   define VERSTR2(x) #x
#   define VERSTR(x) VERSTR2(x)
#   define COMPILERVER " " VERSTR(__SUNPRO_C)
#elif (defined __VERSION__)
#   define COMPILERVER " " __VERSION__
#else
#   define COMPILERVER ""
#endif

#ifndef __DATE__
#define __DATE__ "(Unknown build date)"
#endif

#ifndef __TIME__
#define __TIME__ "(Unknown build time)"
#endif

#ifndef COMPILER
  #if (defined __GNUC__)
    #define COMPILER "GCC"
  #elif (defined _MSC_VER)
    #define COMPILER "Visual Studio"
  #elif (defined __SUNPRO_C)
    #define COMPILER "Sun Studio"
  #else
    #error Please define your platform.
  #endif
#endif

// macro mess so we can turn APPID and APPREV into a string literal...
#define MAKEBUILDVERSTRINGLITERAL2(id, rev) \
    #id ", revision " rev ", built " __DATE__ " " __TIME__ \
    ", by " COMPILER COMPILERVER

#define MAKEBUILDVERSTRINGLITERAL(id, rev) MAKEBUILDVERSTRINGLITERAL2(id, rev)

const char *fatelf_build_version = MAKEBUILDVERSTRINGLITERAL(APPID, APPREV);



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


// Allocate a copy of (str), xfail() on allocation failure.
char *xstrdup(const char *str)
{
    char *retval = (char *) xmalloc(strlen(str) + 1);
    strcpy(retval, str);
    return retval;
} // xstrdup


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


uint64_t xget_file_size(const char *fname, const int fd)
{
    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1)
        xfail("Failed to fstat '%s': %s", fname, strerror(errno));
    return (uint64_t) statbuf.st_size;
} // xget_file_size


static uint8_t copybuf[256 * 1024];

// xfail() on error.
uint64_t xcopyfile(const char *in, const int infd,
                   const char *out, const int outfd)
{
    uint64_t retval = 0;
    ssize_t rc = 0;
    xlseek(in, infd, 0, SEEK_SET);
    while ( (rc = xread(in, infd, copybuf, sizeof (copybuf), 0)) > 0 )
    {
        xwrite(out, outfd, copybuf, rc);
        retval += (uint64_t) rc;
    } // while

    return retval;
} // xcopyfile


static inline uint64_t minui64(const uint64_t a, const uint64_t b)
{
    return (a < b) ? a : b;
} // minui64


void xcopyfile_range(const char *in, const int infd,
                     const char *out, const int outfd,
                     const uint64_t offset, const uint64_t size)
{
    uint64_t remaining = size;
    xlseek(in, infd, (off_t) offset, SEEK_SET);
    while (remaining)
    {
        const size_t cpysize = minui64(remaining, sizeof (copybuf));
        xread(in, infd, copybuf, cpysize, 1);
        xwrite(out, outfd, copybuf, cpysize);
        remaining -= (uint64_t) cpysize;
    } // while
} // xcopyfile_range


void xread_elf_header(const char *fname, const int fd, const uint64_t offset,
                      FATELF_record *record)
{
    const uint8_t magic[4] = { 0x7F, 0x45, 0x4C, 0x46 };
    uint8_t buf[20];  // we only care about the first 20 bytes.
    xlseek(fname, fd, offset, SEEK_SET);
    xread(fname, fd, buf, sizeof (buf), 1);
    if (memcmp(magic, buf, sizeof (magic)) != 0)
        xfail("'%s' is not an ELF binary", fname);

    record->osabi = buf[7];
    record->osabi_version = buf[8];
    record->word_size = buf[4];
    record->byte_order = buf[5];
    record->reserved0 = 0;
    record->reserved1 = 0;
    record->offset = 0;
    record->size = 0;

    if ((record->word_size != FATELF_32BITS) &&
        (record->word_size != FATELF_64BITS))
    {
        xfail("Unexpected word size (%d) in '%s'", record->word_size, fname);
    } // if

    if (record->byte_order == FATELF_BIGENDIAN)
        record->machine = (((uint16_t)buf[18]) << 8) | (((uint16_t)buf[19]));
    else if (record->byte_order == FATELF_LITTLEENDIAN)
        record->machine = (((uint16_t)buf[19]) << 8) | (((uint16_t)buf[18]));
    else
    {
        xfail("Unexpected byte order (%d) in '%s'",
              (int) record->byte_order, fname);
    } // else
} // xread_elf_header


size_t fatelf_header_size(const int bincount)
{
    return (sizeof (FATELF_header) + (sizeof (FATELF_record) * bincount));
} // fatelf_header_size


// Write a uint8_t to a buffer.
static inline uint8_t *putui8(uint8_t *ptr, const uint8_t val)
{
    *(ptr++) = val;
    return ptr;
} // putui8


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


// Read a uint8_t from a buffer.
static inline uint8_t *getui8(uint8_t *ptr, uint8_t *val)
{
    *val = *ptr;
    return ptr + sizeof (*val);
} // getui8


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
    const size_t buflen = FATELF_DISK_FORMAT_SIZE(header->num_records);
    uint8_t *buf = (uint8_t *) xmalloc(buflen);
    uint8_t *ptr = buf;
    int i;

    ptr = putui32(ptr, header->magic);
    ptr = putui16(ptr, header->version);
    ptr = putui8(ptr, header->num_records);
    ptr = putui8(ptr, header->reserved0);

    for (i = 0; i < header->num_records; i++)
    {
        ptr = putui16(ptr, header->records[i].machine);
        ptr = putui8(ptr, header->records[i].osabi);
        ptr = putui8(ptr, header->records[i].osabi_version);
        ptr = putui8(ptr, header->records[i].word_size);
        ptr = putui8(ptr, header->records[i].byte_order);
        ptr = putui8(ptr, header->records[i].reserved0);
        ptr = putui8(ptr, header->records[i].reserved1);
        ptr = putui64(ptr, header->records[i].offset);
        ptr = putui64(ptr, header->records[i].size);
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
    uint8_t bincount = 0;
    uint8_t reserved0 = 0;
    size_t buflen = 0;
    int i = 0;

    xlseek(fname, fd, 0, SEEK_SET);  // just in case.
    xread(fname, fd, buf, sizeof (buf), 1);
    ptr = getui32(ptr, &magic);
    ptr = getui16(ptr, &version);
    ptr = getui8(ptr, &bincount);
    ptr = getui8(ptr, &reserved0);

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
    header->num_records = bincount;
    header->reserved0 = reserved0;

    for (i = 0; i < bincount; i++)
    {
        ptr = getui16(ptr, &header->records[i].machine);
        ptr = getui8(ptr, &header->records[i].osabi);
        ptr = getui8(ptr, &header->records[i].osabi_version);
        ptr = getui8(ptr, &header->records[i].word_size);
        ptr = getui8(ptr, &header->records[i].byte_order);
        ptr = getui8(ptr, &header->records[i].reserved0);
        ptr = getui8(ptr, &header->records[i].reserved1);
        ptr = getui64(ptr, &header->records[i].offset);
        ptr = getui64(ptr, &header->records[i].size);
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


// !!! FIXME: these names/descs aren't set in stone.
// List from: http://www.sco.com/developers/gabi/latest/ch4.eheader.html
static const fatelf_machine_info machines[] =
{
    // MUST BE SORTED BY ID!
    { 0, "none", "No machine" },
    { 1, "m32", "AT&T WE 32100" },
    { 2, "sparc", "SPARC" },
    { 3, "i386", "Intel 80386" },
    { 4, "68k", "Motorola 68000" },
    { 5, "88k", "Motorola 88000" },
    { 7, "860", "Intel 80860" },
    { 8, "mips", "MIPS I" },
    { 9, "s370", "IBM System/370" },
    { 10, "mips-rs3", "MIPS RS3000" },
    { 15, "pa-risc", "Hewlett-Packard PA-RISC" },
    { 17, "vpp500", "Fujitsu VPP500" },
    { 18, "sparc32plus", "Enhanced instruction set SPARC" },
    { 19, "960", "Intel 80960" },
    { 20, "ppc", "PowerPC" },
    { 21, "ppc64", "64-bit PowerPC" },
    { 22, "s390", "IBM System/390" },
    { 36, "v800", "NEC V800" },
    { 37, "fr20", "Fujitsu FR20" },
    { 38, "rh32", "TRW RH-32" },
    { 39, "rce", "Motorola RCE" },
    { 40, "arm", "Advanced RISC Machines ARM" },
    { 41, "alpha", "Digital Alpha" },
    { 42, "sh", "Hitachi SH" },
    { 43, "sparcv9", "SPARC Version 9" },
    { 44, "tricore", "Siemens Tricore embedded" },
    { 45, "arc", "Argonaut RISC Core" },
    { 46, "h8-300", "Hitachi H8/300" },
    { 47, "h8-300h", "Hitachi H8/300H" },
    { 48, "h8s", "Hitachi H8S" },
    { 49, "h8-500", "Hitachi H8/500" },
    { 50, "ia64", "Intel IA-64" },
    { 51, "mipsx", "Stanford MIPS-X" },
    { 52, "coldfire", "Motorola Coldfire" },
    { 53, "m68hc12", "Motorola M68HC12" },
    { 54, "mma", "Fujitsu MMA Multimedia Accelerator" },
    { 55, "pcp", "Siemens PCP" },
    { 56, "ncpu", "Sony nCPU embedded RISC" },
    { 57, "ndr1", "Denso NDR1" },
    { 58, "starcore", "Motorola Star*Core" },
    { 59, "me16", "Toyota ME16" },
    { 60, "st100", "STMicroelectronics ST100" },
    { 61, "tinyj", "Advanced Logic Corp. TinyJ" },
    { 62, "x86_64", "AMD x86-64" },
    { 63, "pdsp", "Sony DSP" },
    { 64, "pdp10", "Digital Equipment Corp. PDP-10" },
    { 65, "pdp11", "Digital Equipment Corp. PDP-11" },
    { 66, "fx66", "Siemens FX66" },
    { 67, "st9plus", "STMicroelectronics ST9+" },
    { 68, "st7", "STMicroelectronics ST7" },
    { 69, "68hc16", "Motorola MC68HC16" },
    { 70, "68hc11", "Motorola MC68HC11" },
    { 70, "68hc11", "Motorola MC68HC11" },
    { 71, "68hc08", "Motorola MC68HC08" },
    { 72, "68hc05", "Motorola MC68HC05" },
    { 73, "svx", "Silicon Graphics SVx" },
    { 74, "st19", "STMicroelectronics ST19" },
    { 75, "vax", "Digital VAX" },
    { 76, "cris", "Axis Communications 32-bit embedded processor" },
    { 77, "javelin", "Infineon Technologies 32-bit embedded processor" },
    { 78, "firepath", "Element 14 64-bit DSP Processor" },
    { 79, "zsp", "LSI Logic 16-bit DSP Processor" },
    { 80, "mmix", "Donald Knuth's educational 64-bit processor" },
    { 81, "huany", "Harvard University machine-independent object files" },
    { 82, "prism", "SiTera Prism" },
    { 83, "avr", "Atmel AVR" },
    { 84, "fr30", "Fujitsu FR30" },
    { 85, "d10v", "Mitsubishi D10V" },
    { 86, "d30v", "Mitsubishi D30V" },
    { 87, "v850", "NEC v850" },
    { 88, "m32r", "Mitsubishi M32R" },
    { 89, "mn10300", "Matsushita MN10300" },
    { 90, "mn10200", "Matsushita MN10200" },
    { 91, "pj", "picoJava" },
    { 92, "openrisc", "OpenRISC" },
    { 93, "arc_a5", "ARC Cores Tangent-A5" },
    { 94, "xtensa", "Tensilica Xtensa" },
    { 95, "videocore", "Alphamosaic VideoCore" },
    { 96, "tmm_gpp", "Thompson Multimedia General Purpose Processor" },
    { 97, "ns32k", "National Semiconductor 32000 series" },
    { 98, "tpc", "Tenor Network TPC" },
    { 99, "snp1k", "Trebia SNP 1000" },
    { 100, "st200", "STMicroelectronics ST200" },
    { 101, "ip2k", "Ubicom IP2xxx" },
    { 102, "max", "MAX Processor" },
    { 103, "cr", "National Semiconductor CompactRISC" },
    { 104, "f2mc16", "Fujitsu F2MC16" },
    { 105, "msp430", "Texas Instruments msp430" },
    { 106, "blackfin", "Analog Devices Blackfin" },
    { 107, "se_c33", "S1C33 Family of Seiko Epson processors" },
    { 108, "sep", "Sharp embedded microprocessor" },
    { 109, "arca", "Arca RISC Microprocessor" },
    { 110, "unicore", "Microprocessor series from PKU-Unity Ltd. and MPRC of Peking University" },
    { 0x9026, "alpha", "Digital Alpha" },  // linux headers use this.
    { 0x9080, "v850", "NEC v850" },  // old tools use this, apparently.
    { 0x9041, "m32r", "Mitsubishi M32R" },  // old tools use this, apparently.
    { 0xA390, "s390", "IBM System/390" },  // legacy value.
    { 0xBEEF, "mn10300", "Matsushita MN10300" },  // old tools.
};


// !!! FIXME: these names/descs aren't set in stone.
// List from: http://www.sco.com/developers/gabi/latest/ch4.eheader.html
static const fatelf_osabi_info osabis[] =
{
    // MUST BE SORTED BY ID!
    { 0, "sysv", "UNIX System V" },
    { 1, "hpux", "Hewlett-Packard HP-UX" },
    { 2, "netbsd", "NetBSD" },
    { 3, "linux", "Linux" },
    { 4, "hurd", "Hurd" },
    { 5, "86open", "86Open common IA32" },
    { 6, "solaris", "Sun Solaris" },
    { 7, "aix", "AIX" },
    { 8, "irix", "IRIX" },
    { 9, "freebsd", "FreeBSD" },
    { 10, "tru64", "Compaq TRU64 UNIX" },
    { 11, "modesto", "Novell Modesto" },
    { 12, "openbsd", "OpenBSD" },
    { 13, "openvms", "OpenVMS" },
    { 14, "nsk", "Hewlett-Packard Non-Stop Kernel" },
    { 15, "aros", "Amiga Research OS" },
    { 97, "armabi", "ARM" },
    { 255, "standalone", "Standalone application" },
};


const fatelf_machine_info *get_machine_by_id(const uint16_t id)
{
    int i;
    for (i = 0; i < (sizeof (machines) / sizeof (machines[0])); i++)
    {
        if (machines[i].id == id)
            return &machines[i];
        else if (machines[i].id > id)
            break;  // not found (sorted by id).
    } // for

    return NULL;
} // get_machine_by_id


const fatelf_machine_info *get_machine_by_name(const char *name)
{
    int i;
    for (i = 0; i < (sizeof (machines) / sizeof (machines[0])); i++)
    {
        if (strcmp(machines[i].name, name) == 0)
            return &machines[i];
    } // for

    return NULL;
} // get_machine_by_name


const fatelf_osabi_info *get_osabi_by_id(const uint8_t id)
{
    int i;
    for (i = 0; i < (sizeof (osabis) / sizeof (osabis[0])); i++)
    {
        if (osabis[i].id == id)
            return &osabis[i];
        else if (osabis[i].id > id)
            break;  // not found (sorted by id).
    } // for

    return NULL;
} // get_osabi_by_id


const fatelf_osabi_info *get_osabi_by_name(const char *name)
{
    int i;
    for (i = 0; i < (sizeof (osabis) / sizeof (osabis[0])); i++)
    {
        if (strcmp(osabis[i].name, name) == 0)
            return &osabis[i];
    } // for

    return NULL;
} // get_osabi_by_name


static int parse_abi_version_string(const char *str)
{
    long num = 0;
    char *endptr = NULL;
    const char *prefix = "osabiver";
    const size_t prefix_len = 8;
    assert(strlen(prefix) == prefix_len);
    if (strncmp(str, prefix, prefix_len) != 0)
        return -1;

    str += prefix_len;
    num = strtol(str, &endptr, 0);
    return ( ((endptr == str) || (*endptr != '\0')) ? -1 : ((int) num) );
} // parse_abi_version_string


static int xfind_fatelf_record_by_fields(const FATELF_header *header,
                                         const char *target)
{
    char *buf = xstrdup(target);
    const fatelf_osabi_info *osabi = NULL;
    const fatelf_machine_info *machine = NULL;
    FATELF_record rec;
    int wants = 0;
    int abiver = 0;
    char *str = buf;
    char *ptr = buf;
    int retval = -1;
    int i = 0;

    while (1)
    {
        const char ch = *ptr;
        if ((ch == ':') || (ch == '\0'))
        {
            *ptr = '\0';

            if (ptr == str)
            {
                // no-op for empty string.
            } // if
            else if ((strcmp(str,"be")==0) || (strcmp(str,"bigendian")==0))
            {
                wants |= FATELF_WANT_BYTEORDER;
                rec.byte_order = FATELF_BIGENDIAN;
            } // if
            else if ((strcmp(str,"le")==0) || (strcmp(str,"littleendian")==0))
            {
                wants |= FATELF_WANT_BYTEORDER;
                rec.byte_order = FATELF_LITTLEENDIAN;
            } // else if
            else if (strcmp(str,"32bit") == 0)
            {
                wants |= FATELF_WANT_WORDSIZE;
                rec.word_size = FATELF_32BITS;
            } // else if
            else if (strcmp(str,"64bit") == 0)
            {
                wants |= FATELF_WANT_WORDSIZE;
                rec.word_size = FATELF_64BITS;
            } // else if
            else if ((machine = get_machine_by_name(str)) != NULL)
            {
                wants |= FATELF_WANT_MACHINE;
                rec.machine = machine->id;
            } // else if
            else if ((osabi = get_osabi_by_name(str)) != NULL)
            {
                wants |= FATELF_WANT_OSABI;
                rec.osabi = osabi->id;
            } // else if
            else if ((abiver = parse_abi_version_string(str)) != -1)
            {
                wants |= FATELF_WANT_OSABIVER;
                rec.osabi_version = (uint8_t) abiver;
            } // else if
            else
            {
                xfail("Unknown target '%s'", str);
            } // else

            if (ch == '\0')
                break;  // we're done.

            str = ptr + 1;
        } // if

        ptr++;
    } // while

    free(buf);

    for (i = 0; i < ((int) header->num_records); i++)
    {
        const FATELF_record *prec = &header->records[i];
        if ((wants & FATELF_WANT_MACHINE) && (rec.machine != prec->machine))
            continue;
        else if ((wants & FATELF_WANT_OSABI) && (rec.osabi != prec->osabi))
            continue;
        else if ((wants & FATELF_WANT_OSABIVER) && (rec.osabi_version != prec->osabi_version))
            continue;
        else if ((wants & FATELF_WANT_WORDSIZE) && (rec.word_size != prec->word_size))
            continue;
        else if ((wants & FATELF_WANT_BYTEORDER) && (rec.byte_order != prec->byte_order))
            continue;

        if (retval != -1)
            xfail("Ambiguous target '%s'", target);
        retval = i;
    } // for

    return retval;
} // xfind_fatelf_record_by_fields


int xfind_fatelf_record(const FATELF_header *header, const char *target)
{
    if (strncmp(target, "record", 6) == 0)
    {
        char *endptr = NULL;
        const long num = strtol(target+6, &endptr, 0);
        if ((endptr != target+6) && (*endptr == '\0'))  // a numeric index?
        {
            const long recs = (long) header->num_records;
            if ((num < 0) || (num > recs))
            {
                xfail("No record #%ld in FatELF header (max %d)",
                      num, (int) recs);
            } // if
            return (int) num;
        } // if
    } // if

    return xfind_fatelf_record_by_fields(header, target);
} // xfind_fatelf_record


int fatelf_record_matches(const FATELF_record *a, const FATELF_record *b)
{
    return ( (a->machine == b->machine) &&
             (a->osabi == b->osabi) &&
             (a->osabi_version == b->osabi_version) &&
             (a->word_size == b->word_size) &&
             (a->byte_order == b->byte_order) );
} // fatelf_record_matches


int find_furthest_record(const FATELF_header *header)
{
    // there's nothing that says the records have to be in order, although
    //  we probably _should_. Just in case, check them all.
    const int total = (int) header->num_records;
    uint64_t furthest = 0;
    int retval = -1;
    int i;

    for (i = 0; i < total; i++)
    {
        const FATELF_record *rec = &header->records[i];
        const uint64_t edge = rec->offset + rec->size;
        if (edge > furthest)
        {
            retval = i;
            furthest = edge;
        } // if
    } // for

    return retval;
} // find_furthest_record


const char *fatelf_get_wordsize_string(const uint8_t wordsize)
{
    if (wordsize == FATELF_32BITS)
        return "32";
    else if (wordsize == FATELF_64BITS)
        return "64";
    return "???";
} // fatelf_get_wordsize_string


const char *fatelf_get_byteorder_name(const uint8_t byteorder)
{
    if (byteorder == FATELF_LITTLEENDIAN)
        return "Littleendian";
    else if (byteorder == FATELF_BIGENDIAN)
        return "Bigendian";
    return "???";
} // get_byteorder_name


const char *fatelf_get_byteorder_target_name(const uint8_t byteorder)
{
    if (byteorder == FATELF_LITTLEENDIAN)
        return "le";
    else if (byteorder == FATELF_BIGENDIAN)
        return "be";
    return NULL;
} // fatelf_get_byteorder_target_name


const char *fatelf_get_wordsize_target_name(const uint8_t wordsize)
{
    if (wordsize == FATELF_32BITS)
        return "32bits";
    else if (wordsize == FATELF_64BITS)
        return "64bits";
    return NULL;
} // fatelf_get_wordsize_target_name



const char *fatelf_get_target_name(const FATELF_record *rec, const int wants)
{
    // !!! FIXME: this code is sort of stinky.
    static char buffer[128];
    const fatelf_osabi_info *osabi = get_osabi_by_id(rec->osabi);
    const fatelf_machine_info *machine = get_machine_by_id(rec->machine);
    const char *order = fatelf_get_byteorder_target_name(rec->byte_order);
    const char *wordsize = fatelf_get_wordsize_target_name(rec->word_size);

    buffer[0] = '\0';

    if ((wants & FATELF_WANT_MACHINE) && (machine))
    {
        if (buffer[0])
            strcat(buffer, ":");
        strcat(buffer, machine->name);
    } // if

    if ((wants & FATELF_WANT_WORDSIZE) && (wordsize))
    {
        if (buffer[0])
            strcat(buffer, ":");
        strcat(buffer, wordsize);
    } // if

    if ((wants & FATELF_WANT_BYTEORDER) && (order))
    {
        if (buffer[0])
            strcat(buffer, ":");
        strcat(buffer, order);
    } // if

    if ((wants & FATELF_WANT_OSABI) && (osabi))
    {
        if (buffer[0])
            strcat(buffer, ":");
        strcat(buffer, osabi->name);
    } // if

    if (wants & FATELF_WANT_OSABIVER)
    {
        char tmp[32];
        if (buffer[0])
            strcat(buffer, ":");
        snprintf(tmp, sizeof (tmp), "osabiver%d", (int) rec->osabi_version);
        strcat(buffer, tmp);
    } // if

    return buffer;
} // fatelf_get_target_name


int xfind_junk(const char *fname, const int fd, const FATELF_header *header,
               uint64_t *offset, uint64_t *size)
{
    const int furthest = find_furthest_record(header);

    if (furthest >= 0)  // presumably, we failed elsewhere, but oh well.
    {
        const uint64_t fsize = xget_file_size(fname, fd);
        const FATELF_record *rec = &header->records[furthest];
        const uint64_t edge = rec->offset + rec->size;
        if (fsize > edge)
        {
            *offset = edge;
            *size = fsize - edge;
            return 1;
        } // if
    } // if

    return 0;
} // xfind_junk


void xappend_junk(const char *fname, const int fd,
                  const char *out, const int outfd,
                  const FATELF_header *header)
{
    uint64_t offset, size;
    if (xfind_junk(fname, fd, header, &offset, &size))
        xcopyfile_range(fname, fd, out, outfd, offset, size);
} // xappend_junk


void xfatelf_init(int argc, const char **argv)
{
    memset(zerobuf, '\0', sizeof (zerobuf));  // just in case.
    if ((argc >= 2) && (strcmp(argv[1], "--version") == 0))
    {
        printf("%s\n", fatelf_build_version);
        exit(0);
    } // if
} // xfatelf_init

// end of fatelf-utils.c ...

