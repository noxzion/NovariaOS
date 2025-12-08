#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <core/kernel/kstd.h>
#include <stdint.h>

#define MULTIBOOT_FLAG_MODS   0x00000008

// Multiboot structure remains 32-bit for compatibility with bootloader
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;  // 32-bit address, need to cast to 64-bit pointer
} multiboot_info_t;

typedef struct module {
    uint32_t mod_start;  // 32-bit address, need to cast to 64-bit pointer
    uint32_t mod_end;    // 32-bit address, need to cast to 64-bit pointer
    uint32_t string;
    uint32_t reserved;
} module_t;

#endif // MULTIBOOT_H