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
        info->machine = (((uint16_t)buf[18]) << 8) | (((uint16_t)buf[19]));
    else if (buf[5] == 1)  // littleendian
        info->machine = (((uint16_t)buf[19]) << 8) | (((uint16_t)buf[18]));
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
        ptr = putui16(ptr, header->binaries[i].machine);
        ptr = putui16(ptr, header->binaries[i].reserved0);
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
        ptr = getui16(ptr, &header->binaries[i].machine);
        ptr = getui16(ptr, &header->binaries[i].reserved0);
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


// !!! FIXME: these names/descs aren't set in stone.
// List from: http://www.sco.com/developers/gabi/latest/ch4.eheader.html
static const fatelf_machine_info machines[] =
{
    // MUST BE SORTED BY ID!
    { 0, "none", "No machine" },
    { 1, "m32", "AT&T WE 32100" },
    { 2, "sparc", "SPARC" },
    { 3, "386", "Intel 80386" },
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
static const fatelf_abi_info abis[] =
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
    { 97, "arm", "ARM" },
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


const fatelf_abi_info *get_abi_by_id(const uint16_t id)
{
    int i;
    for (i = 0; i < (sizeof (abis) / sizeof (abis[0])); i++)
    {
        if (abis[i].id == id)
            return &abis[i];
        else if (abis[i].id > id)
            break;  // not found (sorted by id).
    } // for

    return NULL;
} // get_abi_by_id


const fatelf_abi_info *get_abi_by_name(const char *name)
{
    int i;
    for (i = 0; i < (sizeof (abis) / sizeof (abis[0])); i++)
    {
        if (strcmp(abis[i].name, name) == 0)
            return &abis[i];
    } // for

    return NULL;
} // get_abi_by_name


void xfatelf_init(int argc, const char **argv)
{
    memset(zerobuf, '\0', sizeof (zerobuf));  // just in case.
} // xfatelf_init

// end of fatelf-utils.c ...

