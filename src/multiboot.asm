; Multiboot header
MBOOT_ALIGN     equ 1 << 0
MBOOT_MEMINFO   equ 1 << 1
MBOOT_FLAGS     equ MBOOT_ALIGN | MBOOT_MEMINFO
MBOOT_MAGIC     equ 0x1BADB002
MBOOT_CHECKSUM  equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KB stack
stack_top:

section .text
global _start
extern main

_start:
    ; Set up stack
    mov esp, stack_top
    
    ; Call kernel main
    call main
    
    ; Hang if main returns
    cli
.hang:
    hlt
    jmp .hang
