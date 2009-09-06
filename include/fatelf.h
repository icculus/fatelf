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

#define FATELF_MAGIC (0x00Fa7E1f)
#define FATELF_FORMAT_VERSION (1)

/* This does not count padding for page alignment at the end. */
#define FATELF_DISK_FORMAT_SIZE(bins) (8 + (16 * (bins)))

/* Values on disk are always littleendian, and align like Elf64. */
typedef struct FATELF_binary_info
{
    uint16_t abi;
    uint16_t abi_version;
    uint32_t cpu;
    uint64_t offset;
} FATELF_binary_info;

/* Values on disk are always littleendian, and align like Elf64. */
typedef struct FATELF_header
{
    uint32_t magic;  /* always FATELF_MAGIC */
    uint16_t version; /* latest is always FATELF_FORMAT_VERSION */
    uint16_t num_binaries;
    FATELF_binary_info binaries[];
} FATELF_header;

#endif

/* end of fatelf.h ... */

