// SPDX-License-Identifier: LGPL-3.0-or-later

#include <core/arch/multiboot.h>
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
#include <stddef.h>
#include <stdbool.h>

void kmain(multiboot_info_t* mb_info) {
    enable_cursor();

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

    uint32_t available_memory = mb_info->mem_upper * 1024;
    initializeMemoryManager((void*)0x100000, available_memory);

    init_serial();
    pit_init();
    ramfs_init();
    vfs_init();
    syslog_init();
    
    char buf[16];
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
    
    syslog_write("Checking multiboot modules...\n");
    
    char mod_count_msg[64];
    mod_count_msg[0] = 'M'; mod_count_msg[1] = 'o'; mod_count_msg[2] = 'd'; 
    mod_count_msg[3] = 'u'; mod_count_msg[4] = 'l'; mod_count_msg[5] = 'e'; 
    mod_count_msg[6] = 's'; mod_count_msg[7] = ':'; mod_count_msg[8] = ' ';
    itoa(mb_info->mods_count, buf, 10);
    int j = 0;
    while (buf[j]) {
        mod_count_msg[9 + j] = buf[j];
        j++;
    }
    mod_count_msg[9 + j] = '\n';
    mod_count_msg[10 + j] = '\0';
    syslog_write(mod_count_msg);
    
    if (mb_info->flags & MULTIBOOT_FLAG_MODS && mb_info->mods_count > 0) {
        module_t* modules = (module_t*)mb_info->mods_addr;
        
        // Check each module for ISO9660 signature
        for (uint32_t i = 0; i < mb_info->mods_count; i++) {
            uint32_t mod_start_addr = modules[i].mod_start;
            uint32_t mod_end_addr = modules[i].mod_end;
            
            // Skip invalid modules
            if (mod_start_addr == 0 || mod_end_addr == 0 || mod_end_addr <= mod_start_addr) {
                continue;
            }
            
            void* mod_start = (void*)mod_start_addr;
            uint32_t mod_size = mod_end_addr - mod_start_addr;
            
            char mod_msg[128];
            mod_msg[0] = 'M'; mod_msg[1] = 'o'; mod_msg[2] = 'd'; mod_msg[3] = ' ';
            itoa(i, buf, 10);
            int k = 0;
            while (buf[k]) {
                mod_msg[4 + k] = buf[k];
                k++;
            }
            mod_msg[4 + k] = ':'; mod_msg[5 + k] = ' ';
            itoa(mod_size, buf, 10);
            int m = 0;
            while (buf[m]) {
                mod_msg[6 + k + m] = buf[m];
                m++;
            }
            mod_msg[6 + k + m] = ' ';
            mod_msg[7 + k + m] = 'b';
            mod_msg[8 + k + m] = 'y';
            mod_msg[9 + k + m] = 't';
            mod_msg[10 + k + m] = 'e';
            mod_msg[11 + k + m] = 's';
            mod_msg[12 + k + m] = '\n';
            mod_msg[13 + k + m] = '\0';
            syslog_write(mod_msg);
            
            // Check for CD001 signature at sector 16 (offset 0x8000)
            if (mod_size > 0x8000 + 5) {
                char* sig = (char*)mod_start + 0x8001;
                
                if (sig[0] == 'C' && sig[1] == 'D' && sig[2] == '0' && sig[3] == '0' && sig[4] == '1') {
                    iso_location = mod_start;
                    iso_size = mod_size;
                    syslog_print(":: Found ISO9660 filesystem in module\n", 7);
                    break;
                }
            }
        }
    }
    
    // Method 2: Fast memory scan in low/BIOS cache areas
    if (!iso_location) {
        // Quick scan only critical areas where BIOS might cache CD-ROM
        uint32_t check_addrs[] = {
            0x00100000, // 1MB - standard kernel load area
            0x00200000, // 2MB
            0x00400000, // 4MB
            0x00800000, // 8MB - common GRUB area
            0x01000000, // 16MB
            0x02000000, // 32MB
            0
        };
        
        for (int i = 0; check_addrs[i] != 0; i++) {
            uint32_t addr = check_addrs[i];
            char* potential_iso = (char*)addr;
            
            // Quick check for ISO signature
            if (addr + 0x8005 < 0x10000000) {
                char* sig = potential_iso + 0x8001;
                if (sig[0] == 'C' && sig[1] == 'D' && sig[2] == '0' && 
                    sig[3] == '0' && sig[4] == '1' && potential_iso[0x8000] == 0x01) {
                    
                    iso_location = (void*)addr;
                    uint32_t* vol_size_ptr = (uint32_t*)(potential_iso + 0x8050);
                    iso_size = (*vol_size_ptr) * 2048;
                    if (iso_size == 0 || iso_size > 1024*1024*1024) {
                        iso_size = 50 * 1024 * 1024;
                    }
                    syslog_print(":: Found ISO9660 filesystem in memory\n", 7);
                    break;
                }
            }
        }
    }
    
    if (!iso_location) {
        syslog_write("Scanning memory for ISO...\n");
        
        uint32_t scan_addresses[] = {
            0x00100000,  // 1 MB
            0x00200000,  // 2 MB
            0x00400000,  // 4 MB
            0x00800000,  // 8 MB
            0x01000000,  // 16 MB
            0x02000000,  // 32 MB
            0x04000000,  // 64 MB
            0x08000000,  // 128 MB
        };
        
        for (int i = 0; i < 8; i++) {
            uint32_t addr = scan_addresses[i];
            uint8_t* potential_iso = (uint8_t*)addr;
            
            if (potential_iso[0x8001] == 'C' &&
                potential_iso[0x8002] == 'D' &&
                potential_iso[0x8003] == '0' &&
                potential_iso[0x8004] == '0' &&
                potential_iso[0x8005] == '1') {
                
                iso_location = (void*)addr;
                uint32_t* vol_size_ptr = (uint32_t*)(potential_iso + 0x8050);
                iso_size = (*vol_size_ptr) * 2048;
                
                syslog_write("Found ISO at address: 0x");
                char addr_msg[16];
                itoa(addr, addr_msg, 16);
                syslog_write(addr_msg);
                syslog_write("\n");
                break;
            }
        }
    }
    
    if (iso_location) {
        cdrom_set_iso_data(iso_location, iso_size);
        iso9660_init(iso_location, iso_size);
        syslog_write("ISO9660 filesystem mounted\n");
        
        // Mount ISO root to /bin/
        iso9660_mount_to_vfs("/bin", "/");
        syslog_write("ISO contents mounted to /bin/\n");
    } else {
        syslog_print(":: ISO9660 filesystem not found\n", 14);
    }
    
    initramfs_load(mb_info);
    syslog_write("Initramfs loaded\n");
    
    nvm_init();
    syslog_write("NVM initialized\n");
    
    userspace_init_programs();
    syslog_write("Userspace programs registered\n");
    
    // Execute programs from initramfs
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
    
    // If shell not exits:
    while(true) {
        nvm_scheduler_tick();
    }
}
