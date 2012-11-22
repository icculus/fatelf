/**
 * FatELF; support multiple ELF binaries in one file.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef __INCL_FATELF_H__
#define __INCL_FATELF_H__ 1

#include <stdint.h>

/* This is little endian on disk, and looks like "FA700E1F" in a hex editor. */
#define FATELF_MAGIC (0x1F0E70FA)
#define FATELF_FORMAT_VERSION (1)

/* This does not count padding for page alignment at the end. */
#define FATELF_DISK_FORMAT_SIZE(bins) (8 + (24 * (bins)))

/* Valid FATELF_record::word_size values... */
#define FATELF_32BITS (1)
#define FATELF_64BITS (2)

/* Valid FATELF_record::byte_order values... */
#define FATELF_BIGENDIAN (0)
#define FATELF_LITTLEENDIAN (1)

/* Values on disk are always littleendian, and align like Elf64. */
typedef struct FATELF_record
{
    uint16_t machine;       /* maps to e_machine. */
    uint8_t osabi;          /* maps to e_ident[EI_OSABI]. */ 
    uint8_t osabi_version;  /* maps to e_ident[EI_ABIVERSION]. */
    uint8_t word_size;      /* maps to e_ident[EI_CLASS]. */
    uint8_t byte_order;     /* maps to e_ident[EI_DATA]. */
    uint8_t reserved0;
    uint8_t reserved1;
    uint64_t offset;
    uint64_t size;
} FATELF_record;

/* Values on disk are always littleendian, and align like Elf64. */
typedef struct FATELF_header
{
    uint32_t magic;  /* always FATELF_MAGIC */
    uint16_t version; /* latest is always FATELF_FORMAT_VERSION */
    uint8_t num_records;
    uint8_t reserved0;
    FATELF_record records[0];  /* this is actually num_records items. */
} FATELF_header;

#endif

/* end of fatelf.h ... */

