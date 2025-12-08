// SPDX-License-Identifier: LGPL-3.0-or-later

#include <core/arch/multiboot.h>
#include <core/arch/multiboot2.h>
#include <core/arch/acpi.h>
#include <core/kernel/kstd.h>
#include <core/kernel/mem.h>
#include <core/kernel/nvm/nvm.h>
#include <core/kernel/nvm/caps.h>
#include <core/drivers/serial.h>
#include <core/drivers/vga.h>
#include <core/drivers/timer.h>
#include <core/drivers/keyboard.h>
#include <core/drivers/cdrom.h>
#include <core/kernel/shell.h>
#include <core/kernel/syslog.h>
#include <core/fs/ramfs.h>
#include <core/fs/initramfs.h>
#include <core/fs/iso9660.h>
#include <usr/vfs.h>
#include <usr/userspace_init.h>
#include <usr/mmap.h>
#include <stddef.h>
#include <stdbool.h>

void kmain(uint64_t mb_info_addr) {
    enable_cursor();
    clearscreen();
    
    // Cast 32-bit multiboot2 pointer to 64-bit
    struct multiboot2_info* mb2_info = (struct multiboot2_info*)(uintptr_t)mb_info_addr;
    
    // Save multiboot info for mmap command
    mmap_set_multiboot_info(mb2_info);

    const char* ascii_art[] = {
        " _   _                      _        ___  ____  ",
        "| \\ | | _____   ____ _ _ __(_) __ _ / _ \\/ ___| ",
        "|  \\| |/ _ \\ \\ / / _` | '__| |/ _` | | | \\___ \\ ",
        "| |\\  | (_) \\ V / (_| | |  | | (_| | |_| |___) |",
        "|_| \\_|\\___/ \\_/ \\__,_|_|  |_|\\__,_|\\___/|____/ "
    };

    for (int i = 0; i < sizeof(ascii_art) / sizeof(ascii_art[0]); i++) {
        kprint(ascii_art[i], 15);
        kprint("\n", 15);
    }

    kprint("                                 TG: ", 15);
    kprint("@NovariaOS\n", 9);

    kprint(":: Initializing memory manager...\n", 7);

    // Parse Multiboot2 tags to find memory info
    uint64_t total_available_memory = 0;
    uint64_t highest_usable_address = 0;
    uint64_t largest_block_start = 0;
    uint64_t largest_block_size = 0;
    bool found_mmap = false;
    
    if (mb2_info != NULL) {
        struct multiboot2_tag* tag;
        for (tag = multiboot2_first_tag(mb2_info);
             tag->type != MULTIBOOT2_TAG_TYPE_END;
             tag = multiboot2_next_tag(tag)) {
            
            // Try to use memory map first (more accurate)
            if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP) {
                struct multiboot2_tag_mmap* mmap_tag = (struct multiboot2_tag_mmap*)tag;
                found_mmap = true;
                
                uint32_t entry_count = (mmap_tag->size - sizeof(struct multiboot2_tag_mmap)) / mmap_tag->entry_size;
                for (uint32_t i = 0; i < entry_count; i++) {
                    struct multiboot2_mmap_entry* entry = (struct multiboot2_mmap_entry*)
                        ((uint8_t*)mmap_tag->entries + i * mmap_tag->entry_size);
                    
                    if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE) {
                        // Count ALL available memory (not just above 1MB)
                        total_available_memory += entry->len;
                        
                        // Track the largest contiguous block above 1MB for allocation
                        if (entry->addr >= 0x100000 && entry->len > largest_block_size) {
                            largest_block_start = entry->addr;
                            largest_block_size = entry->len;
                        }
                        
                        if (entry->addr + entry->len > highest_usable_address) {
                            highest_usable_address = entry->addr + entry->len;
                        }
                    }
                }
                break;
            }
        }
        
        // Fallback to basic meminfo if no memory map found
        if (!found_mmap) {
            for (tag = multiboot2_first_tag(mb2_info);
                 tag->type != MULTIBOOT2_TAG_TYPE_END;
                 tag = multiboot2_next_tag(tag)) {
                if (tag->type == MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO) {
                    struct multiboot2_tag_basic_meminfo* meminfo = 
                        (struct multiboot2_tag_basic_meminfo*)tag;
                    total_available_memory = (uint64_t)meminfo->mem_upper * 1024;
                    highest_usable_address = 0x100000 + total_available_memory;
                    kprint(":: Using basic meminfo (fallback)\n", 14);
                    break;
                }
            }
        }
    }
    
    // Default to 16MB if no info found
    if (total_available_memory == 0) {
        total_available_memory = 16 * 1024 * 1024;
        largest_block_size = total_available_memory;
        largest_block_start = 0x100000;
        highest_usable_address = 0x100000 + total_available_memory;
        kprint(":: WARNING: No memory info found, using default 16MB\n", 14);
    }
    
    // Print total memory detected
    char buf[64];
    kprint(":: Total RAM detected: ", 2);
    formatMemorySize(total_available_memory, buf);
    kprint(buf, 2);
    kprint("\n", 2);
    
    // Initialize memory manager using the largest contiguous block
    // For now, we'll use a contiguous region for simplicity
    // In a production system, you'd want to manage multiple regions
    uint64_t memory_start = largest_block_start;
    uint64_t usable_memory = largest_block_size;
    
    // Reserve space for kernel and modules (conservative estimate: first 16MB after 1MB)
    if (memory_start == 0x100000 && usable_memory > 16 * 1024 * 1024) {
        uint64_t kernel_reserve = 16 * 1024 * 1024;
        memory_start += kernel_reserve;
        usable_memory -= kernel_reserve;
    }
    
    // In x86_64, size_t is 64-bit, so we can use full memory
    // The allocator now supports full 64-bit addressing
    uint32_t available_memory = (uint32_t)(usable_memory > 0xFFFFFFFF ? 0xFFFFFFFF : usable_memory);
    
    kprint(":: Usable memory block: ", 7);
    uitoa_hex(memory_start, buf);
    kprint(buf, 7);
    kprint(" - ", 7);
    uitoa_hex(memory_start + usable_memory - 1, buf);
    kprint(buf, 7);
    kprint(" (", 7);
    formatMemorySize(usable_memory, buf);
    kprint(buf, 7);
    kprint(")\n", 7);
    
    initializeMemoryManager((void*)memory_start, (size_t)usable_memory);

    // Detect NVDIMM (persistent memory) via ACPI
    acpi_parse_nfit();

    init_serial();
    pit_init();
    ramfs_init();
    vfs_init();
    syslog_init();
    
    char mem_msg[64];
    mem_msg[0] = 'M'; mem_msg[1] = 'e'; mem_msg[2] = 'm'; mem_msg[3] = 'o';
    mem_msg[4] = 'r'; mem_msg[5] = 'y'; mem_msg[6] = ':'; mem_msg[7] = ' ';
    itoa(available_memory / 1024 / 1024, buf, 10);
    int i = 0;
    while (buf[i]) {
        mem_msg[8 + i] = buf[i];
        i++;
    }
    mem_msg[8 + i] = ' ';
    mem_msg[9 + i] = 'M';
    mem_msg[10 + i] = 'B';
    mem_msg[11 + i] = '\n';
    mem_msg[12 + i] = '\0';
    syslog_write(mem_msg);
    keyboard_init();
    
    syslog_write("System initialization started\n");
    
    cdrom_init();
    
    void* iso_location = NULL;
    size_t iso_size = 0;
    void* initramfs_location = NULL;
    size_t initramfs_size = 0;
    
    // Check multiboot2 modules
    if (mb2_info != NULL) {
        struct multiboot2_tag* tag;
        for (tag = multiboot2_first_tag(mb2_info);
             tag->type != MULTIBOOT2_TAG_TYPE_END;
             tag = multiboot2_next_tag(tag)) {
            if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
                struct multiboot2_tag_module* mod = (struct multiboot2_tag_module*)tag;
                
                uint32_t mod_start_addr = mod->mod_start;
                uint32_t mod_end_addr = mod->mod_end;
                
                if (mod_start_addr == 0 || mod_end_addr == 0 || mod_end_addr <= mod_start_addr) {
                    continue;
                }
                
                void* mod_start = (void*)(uintptr_t)mod_start_addr;
                uint32_t mod_size = mod_end_addr - mod_start_addr;
                
                // Check if it's initramfs (first module)
                if (initramfs_location == NULL) {
                    initramfs_location = mod_start;
                    initramfs_size = mod_size;
                    syslog_write("Found initramfs module\n");
                    continue;
                }
                
                // Check if it's ISO9660
                if (mod_size > 0x8005) {
                    char* sig = (char*)mod_start + 0x8001;
                    if (sig[0] == 'C' && sig[1] == 'D' && sig[2] == '0' && 
                        sig[3] == '0' && sig[4] == '1') {
                        iso_location = mod_start;
                        iso_size = mod_size;
                        syslog_write("Found ISO9660 module\n");
                    }
                }
            }
        }
    }
    
    if (iso_location) {
        cdrom_set_iso_data(iso_location, iso_size);
        iso9660_init(iso_location, iso_size);
        syslog_write("ISO9660 filesystem mounted\n");

        iso9660_mount_to_vfs("/bin", "/");
        syslog_write("ISO contents mounted to /bin/\n");
    } else {
        syslog_print(":: ISO9660 filesystem not found\n", 14);
    }
    
    initramfs_load_from_memory(initramfs_location, initramfs_size);
    syslog_write("Initramfs loaded\n");
    nvm_init();
    syslog_write("NVM initialized\n");
    userspace_init_programs();
    syslog_write("Userspace programs registered\n");
    size_t program_count = initramfs_get_count();
    if (program_count > 0) {
        for (size_t i = 0; i < program_count; i++) {
            struct program* prog = initramfs_get_program(i);
            if (prog && prog->size > 0) {
                char buf[32];
                int n = i;
                char* p = buf;
                if (n == 0) {
                    *p++ = '0';
                } else {
                    char* start = p;
                    while (n > 0) {
                        *p++ = '0' + n % 10;
                        n /= 10;
                    }
                    p--;
                    while (start < p) {
                        char temp = *start;
                        *start = *p;
                        *p = temp;
                        start++;
                        p--;
                    }
                }
                *p = '\0';
                
                nvm_execute((uint8_t*)prog->data, prog->size, (uint16_t[]){CAP_ALL}, 1);
            }
        }
    } else {
        syslog_print(":: No programs found in initramfs\n", 14);
    }
    
    syslog_write("System initialization complete\n");
    shell_init();
    shell_run();
    
    while(true) {
        nvm_scheduler_tick();
    }
}
