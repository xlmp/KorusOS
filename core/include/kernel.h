#ifndef KERNEL_H
#define KERNEL_H

/*
 * kernel.h - Cabeçalho principal do kernel
 * Descrição: Define tipos básicos, macros e protótipos essenciais
 *            utilizados em todo o kernel do sistema operacional.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * TIPOS BÁSICOS
 * ============================================================ */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef uintptr_t uptr;

// Já está correto se usar uintptr_t — ele será 64 bits automaticamente
typedef uintptr_t uptr;   // 8 bytes em x64

// Remover cast explícito para 32 bits em endereços:
// ANTES:  u32 addr = ...
// DEPOIS: u64 addr = ...   (para endereços de memória)

/* ============================================================
 * MACROS ÚTEIS
 * ============================================================ */
#define NULL            ((void*)0)
#define TRUE            1
#define FALSE           0
#define UNUSED(x)       ((void)(x))
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define PACKED          __attribute__((packed))
#define NORETURN        __attribute__((noreturn))

/* ============================================================
 * PORTAS I/O (inline assembly)
 * ============================================================ */

/* Função: outb
 * Descrição: Escreve um byte em uma porta de I/O */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Função: inb
 * Descrição: Lê um byte de uma porta de I/O */
static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Função: outw
 * Descrição: Escreve uma word em uma porta de I/O */
static inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Função: inw
 * Descrição: Lê uma word de uma porta de I/O */
static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Função: outl
 * Descrição: Escreve um dword em uma porta de I/O */
static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Função: inl
 * Descrição: Lê um dword de uma porta de I/O */
static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Função: io_wait
 * Descrição: Aguarda um ciclo de I/O para dispositivos lentos */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* ============================================================
 * ESTRUTURA MULTIBOOT INFO
 * ============================================================ */
typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
} PACKED multiboot_info_t;

/* ============================================================
 * PROTÓTIPOS DO KERNEL PRINCIPAL
 * ============================================================ */
extern "C"
void kernel_main(u32 magic, multiboot_info_t *mbi);
void kernel_panic(const char *msg);
void kernel_log(const char *msg);

#endif /* KERNEL_H */
