; SPDX-License-Identifier: LGPL-3.0-or-later

section .multiboot
align 8
multiboot_header_start:
    dd 0xe85250d6                ; Multiboot2 magic
    dd 0                         ; Architecture: i386
    dd multiboot_header_end - multiboot_header_start  ; Header length
    dd -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))  ; Checksum
    
    ; Request memory map tag
    align 8
    dw 6    ; type = MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    dw 0    ; flags
    dd 16   ; size
    dd 6    ; MULTIBOOT2_TAG_TYPE_MMAP
    
    ; End tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot_header_end:

section .text
bits 32
global start
global inb
global outb
extern kmain

start:
    cli                  ; Disable interrupts
    
    ; Save multiboot2 info pointer to a safe location
    mov [multiboot2_info_addr], ebx
    
    ; Check multiboot2 magic
    cmp eax, 0x36d76289
    jne .no_multiboot
    
    ; Check for long mode support
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29   ; Test if long mode is available
    jz .no_long_mode
    
    ; Setup page tables
    ; Clear page table memory (0x1000-0xB000)
    ; We need: 1 PML4 (4KB) + 1 PDPT (4KB) + 8 PD tables (32KB) = 40KB total
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 10240      ; 40KB / 4 bytes = 10240 dwords
    rep stosd
    
    ; Setup identity mapping for first 512GB (should be enough for most systems)
    ; PML4[0] -> PDPT at 0x2000
    mov edi, 0x1000
    mov dword [edi], 0x2003
    mov dword [edi + 4], 0
    
    ; PDPT - map 512 entries (each entry maps 1GB, total 512GB)
    ; But we'll map only first 8 entries (8GB) to save space
    ; If you need more, increase this number
    mov edi, 0x2000
    mov eax, 0x3003  ; First PD table
    mov ecx, 8       ; Map 8GB (enough for most VMs)
.set_pdpt:
    mov dword [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x1000  ; Next page table
    add edi, 8
    loop .set_pdpt
    
    ; PD entries - use 2MB pages
    ; We need 8 PD tables * 512 entries each = 4096 entries total
    ; Each entry maps 2MB, so 4096 * 2MB = 8GB
    mov edi, 0x3000
    mov ebx, 0x00000083  ; Present, writable, 2MB page
    mov ecx, 4096        ; 4096 * 2MB = 8GB
.set_page_dir:
    mov dword [edi], ebx
    mov dword [edi + 4], 0
    add ebx, 0x200000    ; Next 2MB page
    jnc .no_carry
    inc dword [edi + 4]  ; Handle carry to upper 32 bits
.no_carry:
    add edi, 8
    loop .set_page_dir
    
    ; Set CR3 to point to PML4
    mov edi, 0x1000
    mov cr3, edi
    
    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5) | (1 << 7)  ; PAE + PGE
    mov cr4, eax
    
    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)  ; LME + NXE
    wrmsr
    
    ; Enable paging and protection
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)  ; PG + PE
    mov cr0, eax
    
    lgdt [gdt64.pointer]
    jmp gdt64.code:start64

.no_multiboot:
    mov dword [0xb8000], 0x4f524f45  ; "ER"
    mov dword [0xb8004], 0x4f3a4f52  ; "R:"
    mov dword [0xb8008], 0x4f4e4f20  ; " N"
    mov dword [0xb800c], 0x4f204f6f  ; "o "
    mov dword [0xb8010], 0x4f424f4d  ; "MB"
    hlt

.no_long_mode:
    mov dword [0xb8000], 0x4f524f45  ; "ER"
    mov dword [0xb8004], 0x4f3a4f52  ; "R:"
    mov dword [0xb8008], 0x4f4e4f20  ; " N"
    mov dword [0xb800c], 0x4f204f6f  ; "o "
    mov dword [0xb8010], 0x4f6f4f4c  ; "Lo"
    mov dword [0xb8014], 0x4f674f6e  ; "ng"
    mov dword [0xb8018], 0x4f4d4f20  ; " M"
    mov dword [0xb801c], 0x4f64f6f   ; "od"
    mov dword [0xb8020], 0x4f65      ; "e"
    hlt

bits 64
start64:
    cli
    
    ; Clear segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup stack
    mov rsp, stack_space
    
    ; Load multiboot info pointer
    xor rdi, rdi
    mov edi, dword [rel multiboot2_info_addr]
    
    ; FPU init
    finit
    fldcw [rel fpu_cw]
    
    ; Enable SSE
    mov rax, cr4
    or rax, (1 << 9) | (1 << 10)
    mov cr4, rax
    
    ; Call kernel
    call kmain
    
.loop:
    cli
    hlt
    jmp .loop

; Function to read a byte from a port
inb:
    mov dx, di           ; Get the port from the first argument (rdi)
    in al, dx            ; Read the value from the port
    movzx rax, al        ; Zero-extend al to rax
    ret

; Function to write a byte to a port
outb:
    mov dx, di           ; Get the port from the first argument (rdi)
    mov al, sil          ; Get the value from the second argument (rsi)
    out dx, al           ; Write the value to the port
    ret

section .data
align 8
gdt64:
    dq 0                                    ; Null descriptor
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; Code segment (64-bit)
.data: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41)          ; Data segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

fpu_cw: dw 0x37f
multiboot2_info_addr: dd 0

section .bss
align 16
stack_bottom:
    resb 32768          ; 32 KB for stack (increased for 64-bit)
stack_space:
