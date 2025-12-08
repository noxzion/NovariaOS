#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT2_TAG_TYPE_END               0
#define MULTIBOOT2_TAG_TYPE_CMDLINE           1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME  2
#define MULTIBOOT2_TAG_TYPE_MODULE            3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO     4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV           5
#define MULTIBOOT2_TAG_TYPE_MMAP              6
#define MULTIBOOT2_TAG_TYPE_VBE               7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER       8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS      9
#define MULTIBOOT2_TAG_TYPE_APM               10
#define MULTIBOOT2_TAG_TYPE_EFI32             11
#define MULTIBOOT2_TAG_TYPE_EFI64             12
#define MULTIBOOT2_TAG_TYPE_SMBIOS            13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD          14
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW          15
#define MULTIBOOT2_TAG_TYPE_NETWORK           16
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP          17
#define MULTIBOOT2_TAG_TYPE_EFI_BS            18
#define MULTIBOOT2_TAG_TYPE_EFI32_IH          19
#define MULTIBOOT2_TAG_TYPE_EFI64_IH          20
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR    21

// Memory map entry types
#define MULTIBOOT2_MEMORY_AVAILABLE           1
#define MULTIBOOT2_MEMORY_RESERVED            2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE    3
#define MULTIBOOT2_MEMORY_NVS                 4
#define MULTIBOOT2_MEMORY_BADRAM              5

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot2_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed));

struct multiboot2_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[0];
} __attribute__((packed));

struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot2_mmap_entry entries[0];
} __attribute__((packed));

struct multiboot2_info {
    uint32_t total_size;
    uint32_t reserved;
    struct multiboot2_tag tags[0];
} __attribute__((packed));

static inline struct multiboot2_tag* multiboot2_next_tag(struct multiboot2_tag* tag) {
    uint8_t* addr = (uint8_t*)tag;
    addr += ((tag->size + 7) & ~7);
    return (struct multiboot2_tag*)addr;
}

static inline struct multiboot2_tag* multiboot2_first_tag(struct multiboot2_info* info) {
    return (struct multiboot2_tag*)((uint8_t*)info + 8);
}

#endif // MULTIBOOT2_H
