#ifndef MEMORY_H
#define MEMORY_H

/*
 * memory.h - Cabeçalho do gerenciador de memória
 * Descrição: Define estruturas e protótipos para gerenciamento de
 *            memória virtual, paginação e alocação de páginas.
 */

#include "kernel.h"

/* ============================================================
 * CONSTANTES DE MEMÓRIA
 * ============================================================ */
#define PAGE_SIZE           4096        /* Tamanho de uma página (4 KB) */
#define PAGE_ALIGN(x)       ALIGN(x, PAGE_SIZE)
#define PAGE_ENTRIES        1024        /* Entradas por tabela de páginas */

/* Flags de página */
#define PAGE_PRESENT        (1 << 0)   /* Página presente na memória */
#define PAGE_WRITABLE       (1 << 1)   /* Página com permissão de escrita */
#define PAGE_USER           (1 << 2)   /* Página acessível em user mode */
#define PAGE_WRITETHROUGH   (1 << 3)   /* Write-through cache */
#define PAGE_NOCACHE        (1 << 4)   /* Cache desabilitado */
#define PAGE_ACCESSED       (1 << 5)   /* Página foi acessada */
#define PAGE_DIRTY          (1 << 6)   /* Página foi modificada */

/* Endereços de memória */
#define KERNEL_VIRTUAL_BASE 0xC0000000 /* Base virtual do kernel */
#define HEAP_START          0x00100000 /* Início da heap do kernel (1 MB) */
#define HEAP_MAX_SIZE       (64 * 1024 * 1024) /* Heap máxima: 64 MB */

/* ============================================================
 * ESTRUTURAS DE PAGINAÇÃO
 * ============================================================ */

/* Entrada de tabela de páginas (64-bit para x86_64) */
typedef struct {
    u64 present    : 1;
    u64 writable   : 1;
    u64 user       : 1;
    u64 writethrough: 1;
    u64 nocache    : 1;
    u64 accessed   : 1;
    u64 dirty      : 1;
    u64 pat        : 1;
    u64 global     : 1;
    u64 avail      : 3;
    u64 frame      : 40;  /* Endereço físico do frame (40 bits) */
    u64 avail2     : 11;
    u64 nx         : 1;   /* No execute */
} PACKED page_entry_t;

/* Tabela de páginas (1024 entradas) */
typedef struct {
    page_entry_t entries[PAGE_ENTRIES];
} PACKED page_table_t;

/* Diretório de páginas (1024 entradas) */
typedef struct {
    page_entry_t entries[PAGE_ENTRIES];
} PACKED page_directory_t;

/* ============================================================
 * INFORMAÇÕES DE MEMÓRIA
 * ============================================================ */
typedef struct {
    u32 total_kb;      /* Memória total em kilobytes */
    u32 used_kb;       /* Memória usada em kilobytes */
    u32 free_kb;       /* Memória livre em kilobytes */
    u32 total_pages;   /* Total de páginas físicas */
    u32 used_pages;    /* Páginas em uso */
    u32 free_pages;    /* Páginas livres */
} memory_info_t;

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: init_memory
 * Descrição: Inicializa o sistema de gerenciamento de memória do kernel
 *            configurando paginação e estruturas internas.
 */
void init_memory(u32 mem_upper_kb);

/*
 * Função: alloc_page
 * Descrição: Aloca uma página física de memória e retorna seu endereço.
 *            Retorna 0 se não houver páginas disponíveis.
 */
u32 alloc_page(void);

/*
 * Função: free_page
 * Descrição: Libera uma página física previamente alocada.
 */
void free_page(u32 addr);

/*
 * Função: map_page
 * Descrição: Mapeia um endereço físico para um endereço virtual no
 *            diretório de páginas atual.
 */
void map_page(u32 virt, u32 phys, u32 flags);

/*
 * Função: unmap_page
 * Descrição: Remove o mapeamento de um endereço virtual.
 */
void unmap_page(u32 virt);

/*
 * Função: get_physical_addr
 * Descrição: Retorna o endereço físico correspondente a um endereço virtual.
 */
u32 get_physical_addr(u32 virt);

/*
 * Função: show_memory_usage
 * Descrição: Exibe no terminal as informações de uso de memória do sistema.
 */
void show_memory_usage(void);

/*
 * Função: get_memory_info
 * Descrição: Preenche a estrutura memory_info_t com os dados atuais de memória.
 */
void get_memory_info(memory_info_t *info);

/*
 * Função: kmalloc
 * Descrição: Aloca um bloco de memória no kernel heap.
 */
void *kmalloc(size_t size);

/*
 * Função: kfree
 * Descrição: Libera um bloco de memória previamente alocado com kmalloc.
 */
void kfree(void *ptr);

/*
 * Função: kmemset
 * Descrição: Preenche um bloco de memória com um valor específico.
 */
void *kmemset(void *ptr, int value, size_t size);

/*
 * Função: kmemcpy
 * Descrição: Copia um bloco de memória de origem para destino.
 */
void *kmemcpy(void *dest, const void *src, size_t size);

#endif /* MEMORY_H */
