#ifndef _KSTD_H
#define _KSTD_H

#include <stdbool.h>
#include <stdint.h>

void reverse(char* str, int length);
char* itoa(int num, char* str, int base);
char* uitoa_hex(uint64_t num, char* str);
void kprint(const char *str, int color);
char* strncpy(char *dest, const char *src, unsigned int n);

#endif // _KSTD_H
