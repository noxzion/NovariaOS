#ifndef MMAP_H
#define MMAP_H

#include <core/arch/multiboot2.h>

extern void mmap_main(int argc, char** argv);
extern void mmap_set_multiboot_info(struct multiboot2_info* info);

#endif // MMAP_H
