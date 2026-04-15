MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; x86_64
MULTIBOOT2_LENGTH   equ (header_end - header_start)
MULTIBOOT2_CHECKSUM equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH)

STACK_SIZE equ 0x8000               ; 32 KB de pilha

section .multiboot
align 8
header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd MULTIBOOT2_LENGTH
    dd MULTIBOOT2_CHECKSUM
    ; Tag de fim (obrigatório no Multiboot2)
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

section .text
bits 64
global _start
extern kernel_main

_start:
    mov rsp, stack_top
    xor rbp, rbp
    ; rdi = magic (1º arg), rsi = multiboot_info (2º arg)
    ; O GRUB já coloca esses valores em rdi/rsi no modo 64 bits
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt