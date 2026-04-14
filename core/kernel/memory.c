/*
 * memory.c - Implementação do gerenciador de memória
 * Descrição: Gerenciamento de memória virtual e física do kernel,
 *            incluindo paginação, alocação de páginas e heap do kernel.
 *
 * Técnicas utilizadas:
 *   - Bitmap de páginas físicas para rastrear uso
 *   - Diretório de páginas com tabelas de páginas
 *   - Heap simples com blocos encadeados (malloc/free)
 */

#include "../include/memory.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(u32 val);

/* ============================================================
 * VARIÁVEIS INTERNAS
 * ============================================================ */

/* Bitmap de frames físicos (1 bit por página de 4KB) */
#define MAX_FRAMES      (256 * 1024)    /* Suporte até 1 GB de RAM */
static u32 frame_bitmap[MAX_FRAMES / 32]; /* 1 bit = 1 frame */

static u32 total_frames  = 0;   /* Total de frames físicos */
static u32 used_frames   = 0;   /* Frames atualmente em uso */
static u32 mem_total_kb  = 0;   /* Memória total em KB */

/* Diretório de páginas do kernel */
static page_directory_t kernel_page_dir __attribute__((aligned(4096)));

/* ============================================================
 * HEAP SIMPLES DO KERNEL
 * ============================================================ */
typedef struct heap_block {
    u32              magic;     /* Valor mágico para detecção de corrupção */
    u32              size;      /* Tamanho do bloco de dados */
    bool             free;      /* Bloco está livre? */
    struct heap_block *next;    /* Próximo bloco na lista */
} heap_block_t;

#define HEAP_MAGIC      0xCAFEBABE
static u8  *heap_base_ptr = NULL;
static u32  heap_current  = 0;
static heap_block_t *heap_head = NULL;

/* ============================================================
 * FUNÇÕES DO BITMAP DE FRAMES
 * ============================================================ */

/*
 * Função: frame_set
 * Descrição: Marca um frame físico como em uso no bitmap.
 */
static void frame_set(u32 frame_addr) {
    u32 frame = frame_addr / PAGE_SIZE;
    frame_bitmap[frame / 32] |= (1 << (frame % 32));
}

/*
 * Função: frame_clear
 * Descrição: Marca um frame físico como livre no bitmap.
 */
static void frame_clear(u32 frame_addr) {
    u32 frame = frame_addr / PAGE_SIZE;
    frame_bitmap[frame / 32] &= ~(1 << (frame % 32));
}

/*
 * Função: frame_test
 * Descrição: Verifica se um frame está em uso (retorna 1) ou livre (0).
 */
static int frame_test(u32 frame_addr) {
    u32 frame = frame_addr / PAGE_SIZE;
    return (frame_bitmap[frame / 32] >> (frame % 32)) & 1;
}

/*
 * Função: frame_find_free
 * Descrição: Encontra e retorna o endereço do primeiro frame livre
 *            disponível no bitmap. Retorna 0 se não houver frames livres.
 */
static u32 frame_find_free(void) {
    for (u32 i = 0; i < total_frames / 32; i++) {
        if (frame_bitmap[i] != 0xFFFFFFFF) {
            for (u32 j = 0; j < 32; j++) {
                if (!(frame_bitmap[i] & (1 << j))) {
                    return (i * 32 + j) * PAGE_SIZE;
                }
            }
        }
    }
    return 0; /* Sem frames disponíveis */
}

/* ============================================================
 * IMPLEMENTAÇÃO DAS FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_memory
 * Descrição: Inicializa o sistema de gerenciamento de memória do kernel
 *            configurando paginação e estruturas internas.
 */
void init_memory(u32 mem_upper_kb) {
    mem_total_kb = mem_upper_kb;
    total_frames = (mem_upper_kb * 1024) / PAGE_SIZE;

    if (total_frames > MAX_FRAMES)
        total_frames = MAX_FRAMES;

    /* Zera o bitmap - todos os frames livres inicialmente */
    kmemset(frame_bitmap, 0, sizeof(frame_bitmap));

    /* Reserva os primeiros 2 MB para o kernel e estruturas críticas */
    u32 reserved_frames = (2 * 1024 * 1024) / PAGE_SIZE;
    for (u32 i = 0; i < reserved_frames; i++) {
        frame_set(i * PAGE_SIZE);
        used_frames++;
    }

    /* Inicializa a heap do kernel após o código do kernel */
    heap_base_ptr = (u8 *)HEAP_START;
    heap_current  = 0;

    /* Cria bloco inicial vazio na heap */
    heap_head = (heap_block_t *)heap_base_ptr;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size  = HEAP_MAX_SIZE - sizeof(heap_block_t);
    heap_head->free  = TRUE;
    heap_head->next  = NULL;

    /* Configura o diretório de páginas do kernel (identity mapping simples) */
    kmemset(&kernel_page_dir, 0, sizeof(kernel_page_dir));

    /* Mapeia os primeiros 4 MB em identity mapping para o kernel */
    for (u32 addr = 0; addr < 4 * 1024 * 1024; addr += PAGE_SIZE) {
        map_page(addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }
}

/*
 * Função: alloc_page
 * Descrição: Aloca uma página física de memória e retorna seu endereço.
 *            Retorna 0 se não houver páginas disponíveis.
 */
u32 alloc_page(void) {
    u32 frame = frame_find_free();
    if (frame == 0) return 0;

    frame_set(frame);
    used_frames++;
    kmemset((void *)frame, 0, PAGE_SIZE);
    return frame;
}

/*
 * Função: free_page
 * Descrição: Libera uma página física previamente alocada.
 */
void free_page(u32 addr) {
    if (addr < 2 * 1024 * 1024) return; /* Não libera área do kernel */
    if (frame_test(addr)) {
        frame_clear(addr);
        used_frames--;
    }
}

/*
 * Função: map_page
 * Descrição: Mapeia um endereço físico para um endereço virtual no
 *            diretório de páginas atual.
 */
void map_page(u32 virt, u32 phys, u32 flags) {
    u32 dir_idx   = virt >> 22;          /* Índice no diretório (10 bits) */
    u32 table_idx = (virt >> 12) & 0x3FF;/* Índice na tabela (10 bits) */

    page_entry_t *dir_entry = &kernel_page_dir.entries[dir_idx];

    /* Se a tabela de páginas não existe, aloca uma nova */
    if (!dir_entry->present) {
        u32 new_table = alloc_page();
        if (!new_table) return; /* Sem memória */

        dir_entry->frame    = new_table >> 12;
        dir_entry->present  = 1;
        dir_entry->writable = 1;
        dir_entry->user     = (flags & PAGE_USER) ? 1 : 0;
    }

    /* Obtém a tabela de páginas */
    page_table_t *table = (page_table_t *)(dir_entry->frame << 12);
    page_entry_t *entry = &table->entries[table_idx];

    entry->frame    = phys >> 12;
    entry->present  = (flags & PAGE_PRESENT)  ? 1 : 0;
    entry->writable = (flags & PAGE_WRITABLE) ? 1 : 0;
    entry->user     = (flags & PAGE_USER)     ? 1 : 0;
}

/*
 * Função: unmap_page
 * Descrição: Remove o mapeamento de um endereço virtual.
 */
void unmap_page(u32 virt) {
    u32 dir_idx   = virt >> 22;
    u32 table_idx = (virt >> 12) & 0x3FF;

    page_entry_t *dir_entry = &kernel_page_dir.entries[dir_idx];
    if (!dir_entry->present) return;

    page_table_t *table = (page_table_t *)(dir_entry->frame << 12);
    page_entry_t *entry = &table->entries[table_idx];
    entry->present = 0;

    /* Invalida TLB para o endereço */
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

/*
 * Função: get_memory_info
 * Descrição: Preenche a estrutura memory_info_t com os dados atuais de memória.
 */
void get_memory_info(memory_info_t *info) {
    info->total_kb    = mem_total_kb;
    info->total_pages = total_frames;
    info->used_pages  = used_frames;
    info->free_pages  = total_frames - used_frames;
    info->used_kb     = used_frames * (PAGE_SIZE / 1024);
    info->free_kb     = info->free_pages * (PAGE_SIZE / 1024);
}

/*
 * Função: show_memory_usage
 * Descrição: Exibe no terminal as informações de uso de memória do sistema.
 */
void show_memory_usage(void) {
    memory_info_t info;
    get_memory_info(&info);

    screen_println("=== Informacoes de Memoria ===");
    screen_print("  Memoria Total : ");
    screen_print_int((s32)(info.total_kb / 1024));
    screen_println(" MB");
    screen_print("  Memoria Usada : ");
    screen_print_int((s32)(info.used_kb / 1024));
    screen_println(" MB");
    screen_print("  Memoria Livre : ");
    screen_print_int((s32)(info.free_kb / 1024));
    screen_println(" MB");
    screen_print("  Paginas Totais: ");
    screen_print_int((s32)info.total_pages);
    screen_println("");
    screen_print("  Paginas Usadas: ");
    screen_print_int((s32)info.used_pages);
    screen_println("");
    screen_print("  Paginas Livres: ");
    screen_print_int((s32)info.free_pages);
    screen_println("");
}

/* ============================================================
 * HEAP DO KERNEL - kmalloc/kfree
 * ============================================================ */

/*
 * Função: kmalloc
 * Descrição: Aloca um bloco de memória no kernel heap.
 *            Usa estratégia first-fit para encontrar bloco livre.
 */
void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Alinha o tamanho para múltiplo de 4 bytes */
    size = ALIGN(size, 4);

    heap_block_t *block = heap_head;
    while (block) {
        if (block->free && block->size >= size && block->magic == HEAP_MAGIC) {
            /* Divide o bloco se houver espaço suficiente para outro bloco */
            if (block->size >= size + sizeof(heap_block_t) + 16) {
                heap_block_t *new_block =
                    (heap_block_t *)((u8 *)block + sizeof(heap_block_t) + size);
                new_block->magic = HEAP_MAGIC;
                new_block->size  = block->size - size - sizeof(heap_block_t);
                new_block->free  = TRUE;
                new_block->next  = block->next;

                block->size = size;
                block->next = new_block;
            }

            block->free = FALSE;
            return (void *)((u8 *)block + sizeof(heap_block_t));
        }
        block = block->next;
    }

    return NULL; /* Heap esgotada */
}

/*
 * Função: kfree
 * Descrição: Libera um bloco de memória previamente alocado com kmalloc.
 *            Funde blocos adjacentes livres para reduzir fragmentação.
 */
void kfree(void *ptr) {
    if (!ptr) return;

    heap_block_t *block = (heap_block_t *)((u8 *)ptr - sizeof(heap_block_t));
    if (block->magic != HEAP_MAGIC) return; /* Ponteiro inválido */

    block->free = TRUE;

    /* Fundir com próximo bloco se também estiver livre */
    if (block->next && block->next->free && block->next->magic == HEAP_MAGIC) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next  = block->next->next;
    }
}

/*
 * Função: kmemset
 * Descrição: Preenche um bloco de memória com um valor específico.
 */
void *kmemset(void *ptr, int value, size_t size) {
    u8 *p = (u8 *)ptr;
    while (size--) *p++ = (u8)value;
    return ptr;
}

/*
 * Função: kmemcpy
 * Descrição: Copia um bloco de memória de origem para destino.
 */
void *kmemcpy(void *dest, const void *src, size_t size) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (size--) *d++ = *s++;
    return dest;
}

void memcpy(void *dest, const void *src, size_t size) {
    kmemcpy(dest, src, size);
}
