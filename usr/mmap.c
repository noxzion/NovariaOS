// SPDX-License-Identifier: LGPL-3.0-or-later

#include <core/arch/multiboot2.h>
#include <core/arch/acpi.h>
#include <core/kernel/kstd.h>
#include <core/kernel/mem.h>
#include <usr/userspace_init.h>
#include <stdbool.h>

// Global pointer to multiboot2 info (set by kernel)
static struct multiboot2_info* g_mb2_info = NULL;

void mmap_set_multiboot_info(struct multiboot2_info* info) {
    g_mb2_info = info;
}

static void print_ram_map(void) {
    if (!g_mb2_info) {
        kprint("Memory map not available\n", 14);
        return;
    }
    
    kprint("RAM Memory Map:\n", 7);
    kprint("================\n", 7);
    
    struct multiboot2_tag* tag;
    for (tag = multiboot2_first_tag(g_mb2_info);
         tag->type != MULTIBOOT2_TAG_TYPE_END;
         tag = multiboot2_next_tag(tag)) {
        
        if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP) {
            struct multiboot2_tag_mmap* mmap_tag = (struct multiboot2_tag_mmap*)tag;
            
            uint32_t entry_count = (mmap_tag->size - sizeof(struct multiboot2_tag_mmap)) / mmap_tag->entry_size;
            for (uint32_t i = 0; i < entry_count; i++) {
                struct multiboot2_mmap_entry* entry = (struct multiboot2_mmap_entry*)
                    ((uint8_t*)mmap_tag->entries + i * mmap_tag->entry_size);
                
                char buf[64];
                kprint("  [", 7);
                uitoa_hex(entry->addr, buf);
                kprint(buf, 7);
                kprint(" - ", 7);
                uitoa_hex(entry->addr + entry->len - 1, buf);
                kprint(buf, 7);
                kprint("] ", 7);
                
                formatMemorySize(entry->len, buf);
                kprint(buf, 7);
                kprint(" - ", 7);
                
                if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE) {
                    kprint("Available\n", 2);
                } else if (entry->type == MULTIBOOT2_MEMORY_RESERVED) {
                    kprint("Reserved\n", 7);
                } else if (entry->type == MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE) {
                    kprint("ACPI Reclaimable\n", 7);
                } else if (entry->type == MULTIBOOT2_MEMORY_NVS) {
                    kprint("ACPI NVS\n", 7);
                } else if (entry->type == MULTIBOOT2_MEMORY_BADRAM) {
                    kprint("Bad RAM\n", 4);
                } else {
                    kprint("Unknown\n", 14);
                }
            }
            break;
        }
    }
}

static void print_nvdimm_map(void) {
    struct acpi_nfit* nfit = (struct acpi_nfit*)acpi_find_table("NFIT");
    
    if (!nfit) {
        kprint("NVDIMM not found (ACPI NFIT table missing)\n", 14);
        return;
    }
    
    kprint("NVDIMM Memory Map:\n", 7);
    kprint("==================\n", 7);
    
    uint8_t* ptr = (uint8_t*)nfit + sizeof(struct acpi_nfit);
    uint8_t* end = (uint8_t*)nfit + nfit->header.length;
    
    bool found = false;
    while (ptr < end) {
        struct acpi_nfit_header* header = (struct acpi_nfit_header*)ptr;
        
        if (header->type == ACPI_NFIT_TYPE_SYSTEM_ADDRESS) {
            struct acpi_nfit_system_address* spa = (struct acpi_nfit_system_address*)ptr;
            
            char buf[64];
            kprint("  [", 7);
            uitoa_hex(spa->address, buf);
            kprint(buf, 7);
            kprint(" - ", 7);
            uitoa_hex(spa->address + spa->length - 1, buf);
            kprint(buf, 7);
            kprint("] ", 7);
            formatMemorySize(spa->length, buf);
            kprint(buf, 7);
            kprint(" - Persistent Memory\n", 2);
            
            found = true;
        }
        
        ptr += header->length;
    }
    
    if (!found) {
        kprint("No NVDIMM regions found\n", 14);
    }
}

void mmap_main(int argc, char** argv) {
    if (argc < 2) {
        kprint("Usage: mmap [-ram|-nvdimm]\n", 7);
        kprint("  -ram     Show RAM memory map\n", 7);
        kprint("  -nvdimm  Show NVDIMM (persistent memory) map\n", 7);
        return;
    }
    
    // Simple string comparison
    const char* arg = argv[1];
    
    if (arg[0] == '-' && arg[1] == 'r' && arg[2] == 'a' && arg[3] == 'm' && arg[4] == '\0') {
        print_ram_map();
    } else if (arg[0] == '-' && arg[1] == 'n' && arg[2] == 'v' && arg[3] == 'd' && 
               arg[4] == 'i' && arg[5] == 'm' && arg[6] == 'm' && arg[7] == '\0') {
        print_nvdimm_map();
    } else {
        kprint("Unknown option: ", 14);
        kprint(arg, 14);
        kprint("\nUse: mmap [-ram|-nvdimm]\n", 7);
    }
}
