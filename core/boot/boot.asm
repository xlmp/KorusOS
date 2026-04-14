; =============================================================================
; boot.asm - Código de boot do OS experimental
; Descrição: Configura o cabeçalho Multiboot para o GRUB e prepara o ambiente
;            mínimo necessário para transferir controle ao kernel em C.
; Arquitetura: x86 / x86_64
; Compilador: NASM ou Clang
; =============================================================================

; Constantes Multiboot
MULTIBOOT_MAGIC         equ 0x1BADB002
MULTIBOOT_ALIGN         equ 1 << 0          ; Alinha módulos em páginas
MULTIBOOT_MEMINFO       equ 1 << 1          ; Informa mapa de memória
MULTIBOOT_FLAGS         equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM      equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Tamanho da pilha inicial (16 KB)
STACK_SIZE              equ 0x4000

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

section .text
global _start
extern kernel_main

; =============================================================================
; _start - Ponto de entrada do sistema
; Descrição: Inicializa a pilha e chama o kernel principal em C.
;            Recebe do GRUB: eax = magic number, ebx = endereço do multiboot info
; =============================================================================
_start:
    ; Configura a pilha
    mov esp, stack_top

    ; Salva os registradores do multiboot para passar ao kernel
    push ebx            ; ponteiro para multiboot_info
    push eax            ; magic number do multiboot

    ; Zera os registradores de segmento de flags
    xor ebp, ebp

    ; Chama o kernel principal escrito em C
    call kernel_main

    ; Se o kernel retornar, entra em loop infinito
.halt:
    cli
    hlt
    jmp .halt
