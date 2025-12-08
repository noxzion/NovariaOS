// SPDX-License-Identifier: LGPL-3.0-or-later

#include <core/arch/acpi.h>
#include <core/kernel/kstd.h>
#include <core/kernel/mem.h>
#include <stdbool.h>
#include <stddef.h>

static uint64_t total_nvdimm_size = 0;

// Search for RSDP in BIOS memory areas
struct acpi_rsdp* acpi_find_rsdp(void) {
    // RSDP is located in:
    // 1. First 1KB of EBDA (Extended BIOS Data Area)
    // 2. Between 0xE0000 and 0xFFFFF (BIOS read-only memory)
    
    // For simplicity, we'll search the common BIOS area
    uint8_t* search_start = (uint8_t*)0xE0000;
    uint8_t* search_end = (uint8_t*)0xFFFFF;
    
    for (uint8_t* addr = search_start; addr < search_end; addr += 16) {
        struct acpi_rsdp* rsdp = (struct acpi_rsdp*)addr;
        
        // Check signature "RSD PTR "
        if (rsdp->signature[0] == 'R' && rsdp->signature[1] == 'S' &&
            rsdp->signature[2] == 'D' && rsdp->signature[3] == ' ' &&
            rsdp->signature[4] == 'P' && rsdp->signature[5] == 'T' &&
            rsdp->signature[6] == 'R' && rsdp->signature[7] == ' ') {
            
            // Verify checksum
            uint8_t sum = 0;
            for (int i = 0; i < 20; i++) {
                sum += ((uint8_t*)rsdp)[i];
            }
            
            if (sum == 0) {
                return rsdp;
            }
        }
    }
    
    return NULL;
}

// Find ACPI table by signature
struct acpi_sdt_header* acpi_find_table(const char* signature) {
    struct acpi_rsdp* rsdp = acpi_find_rsdp();
    if (!rsdp) {
        return NULL;
    }
    
    // Check if we have XSDT (ACPI 2.0+)
    bool use_xsdt = false;
    if (rsdp->revision >= 2) {
        struct acpi_rsdp2* rsdp2 = (struct acpi_rsdp2*)rsdp;
        if (rsdp2->xsdt_address != 0) {
            use_xsdt = true;
        }
    }
    
    struct acpi_sdt_header* root_sdt;
    uint32_t entry_count;
    
    if (use_xsdt) {
        struct acpi_rsdp2* rsdp2 = (struct acpi_rsdp2*)rsdp;
        root_sdt = (struct acpi_sdt_header*)(uintptr_t)rsdp2->xsdt_address;
        entry_count = (root_sdt->length - sizeof(struct acpi_sdt_header)) / 8;
        
        uint64_t* entries = (uint64_t*)((uint8_t*)root_sdt + sizeof(struct acpi_sdt_header));
        
        for (uint32_t i = 0; i < entry_count; i++) {
            struct acpi_sdt_header* header = (struct acpi_sdt_header*)(uintptr_t)entries[i];
            
            if (header->signature[0] == signature[0] &&
                header->signature[1] == signature[1] &&
                header->signature[2] == signature[2] &&
                header->signature[3] == signature[3]) {
                return header;
            }
        }
    } else {
        root_sdt = (struct acpi_sdt_header*)(uintptr_t)rsdp->rsdt_address;
        entry_count = (root_sdt->length - sizeof(struct acpi_sdt_header)) / 4;
        
        uint32_t* entries = (uint32_t*)((uint8_t*)root_sdt + sizeof(struct acpi_sdt_header));
        
        for (uint32_t i = 0; i < entry_count; i++) {
            struct acpi_sdt_header* header = (struct acpi_sdt_header*)(uintptr_t)entries[i];
            
            if (header->signature[0] == signature[0] &&
                header->signature[1] == signature[1] &&
                header->signature[2] == signature[2] &&
                header->signature[3] == signature[3]) {
                return header;
            }
        }
    }
    
    return NULL;
}

// Parse NFIT table to detect NVDIMM
void acpi_parse_nfit(void) {
    struct acpi_nfit* nfit = (struct acpi_nfit*)acpi_find_table("NFIT");
    
    if (!nfit) {
        return;
    }
    
    total_nvdimm_size = 0;
    
    uint8_t* ptr = (uint8_t*)nfit + sizeof(struct acpi_nfit);
    uint8_t* end = (uint8_t*)nfit + nfit->header.length;
    
    while (ptr < end) {
        struct acpi_nfit_header* header = (struct acpi_nfit_header*)ptr;
        
        if (header->type == ACPI_NFIT_TYPE_SYSTEM_ADDRESS) {
            struct acpi_nfit_system_address* spa = (struct acpi_nfit_system_address*)ptr;
            total_nvdimm_size += spa->length;
        }
        
        ptr += header->length;
    }
    
    if (total_nvdimm_size > 0) {
        char buf[64];
        kprint(":: Total NVDIMM detected: ", 2);
        formatMemorySize(total_nvdimm_size, buf);
        kprint(buf, 2);
        kprint("\n", 2);
    }
}

uint64_t acpi_get_nvdimm_size(void) {
    return total_nvdimm_size;
}
