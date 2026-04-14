/*
 * elf_loader.c - Carregador de executáveis ELF
 * Descrição: Implementação do carregador de binários no formato ELF
 *            (Executable and Linkable Format), validando e mapeando
 *            os segmentos carregáveis do programa na memória virtual.
 */

#include "../include/elf.h"
#include "../include/memory.h"
#include "../include/vfs.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_hex(u32 val);
extern void screen_print_int(s32 val);

/* ============================================================
 * FUNÇÕES AUXILIARES INTERNAS
 * ============================================================ */

/*
 * Função: elf_get_header
 * Descrição: Retorna o ponteiro para o cabeçalho ELF no início do buffer.
 */
static const elf32_header_t *elf_get_header(const u8 *data) {
    return (const elf32_header_t *)data;
}

/*
 * Função: elf_get_phdr
 * Descrição: Retorna o ponteiro para o Program Header de índice 'i'.
 */
static const elf32_phdr_t *elf_get_phdr(const elf32_header_t *hdr,
                                         const u8 *data, u32 i) {
    return (const elf32_phdr_t *)(data + hdr->e_phoff +
                                   i * hdr->e_phentsize);
}

/* ============================================================
 * FUNÇÕES PÚBLICAS DO ELF LOADER
 * ============================================================ */

/*
 * Função: validate_elf
 * Descrição: Verifica se o buffer fornecido contém um ELF válido,
 *            checando o magic number, classe, tipo e arquitetura.
 *            Retorna true se válido, false caso contrário.
 */
bool validate_elf(const u8 *data, u32 size) {
    if (!data || size < sizeof(elf32_header_t)) {
        return FALSE;
    }

    const elf32_header_t *hdr = elf_get_header(data);

    /* Verifica magic number: \x7FELF */
    if (hdr->e_ident[0] != 0x7F ||
        hdr->e_ident[1] != 'E'  ||
        hdr->e_ident[2] != 'L'  ||
        hdr->e_ident[3] != 'F') {
        return FALSE;
    }

    /* Verifica classe ELF (só suportamos ELF32) */
    if (hdr->e_ident[4] != ELF_CLASS_32) {
        return FALSE;
    }

    /* Verifica encoding (apenas little-endian) */
    if (hdr->e_ident[5] != ELF_DATA_LSB) {
        return FALSE;
    }

    /* Verifica versão */
    if (hdr->e_version != ELF_VERSION) {
        return FALSE;
    }

    /* Verifica tipo (deve ser executável ou library) */
    if (hdr->e_type != ELF_TYPE_EXEC && hdr->e_type != ELF_TYPE_DYN) {
        return FALSE;
    }

    /* Verifica arquitetura */
    if (hdr->e_machine != ELF_ARCH_386) {
        return FALSE;
    }

    /* Verifica se o Program Header Table cabe no arquivo */
    if (hdr->e_phoff + (u32)(hdr->e_phnum * hdr->e_phentsize) > size) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Função: map_elf_segments
 * Descrição: Percorre os Program Headers do ELF e mapeia cada segmento
 *            do tipo LOAD na memória virtual do processo.
 *            Respeita as flags de permissão de cada segmento.
 */
int map_elf_segments(const elf32_header_t *hdr, const u8 *data) {
    for (u16 i = 0; i < hdr->e_phnum; i++) {
        const elf32_phdr_t *phdr = elf_get_phdr(hdr, data, i);

        /* Processa apenas segmentos LOAD */
        if (phdr->p_type != ELF_PHDR_LOAD) continue;
        if (phdr->p_memsz == 0)            continue;

        /* Determina flags de página */
        u32 page_flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & 0x02) page_flags |= PAGE_WRITABLE; /* PF_W */

        /* Mapeia o segmento página por página */
        u32 vaddr = phdr->p_vaddr & ~(PAGE_SIZE - 1); /* Alinha para baixo */
        u32 vend  = PAGE_ALIGN(phdr->p_vaddr + phdr->p_memsz);

        for (u32 va = vaddr; va < vend; va += PAGE_SIZE) {
            u32 phys = alloc_page();
            if (!phys) return -1; /* Sem memória */

            map_page(va, phys, page_flags);
        }

        /* Copia os dados do arquivo para a memória mapeada */
        if (phdr->p_filesz > 0) {
            kmemcpy((void *)phdr->p_vaddr,
                    data + phdr->p_offset,
                    phdr->p_filesz);
        }

        /* Zera a região BSS (memsz > filesz) */
        if (phdr->p_memsz > phdr->p_filesz) {
            kmemset((void *)(phdr->p_vaddr + phdr->p_filesz),
                    0,
                    phdr->p_memsz - phdr->p_filesz);
        }
    }

    return 0;
}

/*
 * Função: load_elf
 * Descrição: Carrega um executável ELF presente no buffer de dados
 *            para a memória virtual, mapeando todos os segmentos LOAD.
 *            Preenche o contexto com endereço de entrada e informações.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int load_elf(const u8 *data, u32 size, elf_context_t *ctx) {
    if (!ctx) return -1;
    ctx->valid = FALSE;

    /* Valida o binário ELF */
    if (!validate_elf(data, size)) {
        return -2; /* ELF inválido */
    }

    const elf32_header_t *hdr = elf_get_header(data);

    /* Determina endereço base e fim do carregamento */
    u32 load_base = 0xFFFFFFFF;
    u32 load_end  = 0;

    for (u16 i = 0; i < hdr->e_phnum; i++) {
        const elf32_phdr_t *phdr = elf_get_phdr(hdr, data, i);
        if (phdr->p_type != ELF_PHDR_LOAD) continue;

        if (phdr->p_vaddr < load_base) load_base = phdr->p_vaddr;
        u32 seg_end = phdr->p_vaddr + phdr->p_memsz;
        if (seg_end > load_end) load_end = seg_end;
    }

    /* Mapeia os segmentos na memória */
    int result = map_elf_segments(hdr, data);
    if (result != 0) return result;

    /* Preenche o contexto de execução */
    ctx->entry_point = hdr->e_entry;
    ctx->load_base   = load_base;
    ctx->load_end    = load_end;
    ctx->valid       = TRUE;

    return 0;
}

/*
 * Função: load_elf_from_file
 * Descrição: Carrega um executável ELF a partir de um caminho no VFS.
 *            Lê o arquivo, valida e carrega o binário.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int load_elf_from_file(const char *path, elf_context_t *ctx) {
    if (!path || !ctx) return -1;

    /* Abre o arquivo via VFS */
    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        return -2; /* Arquivo não encontrado */
    }

    /* Aloca buffer para o arquivo (limitado a 1 MB por simplicidade) */
    const u32 MAX_ELF_SIZE = 1024 * 1024;
    u8 *buffer = (u8 *)kmalloc(MAX_ELF_SIZE);
    if (!buffer) {
        vfs_close(fd);
        return -3; /* Sem memória */
    }

    /* Lê o arquivo completo */
    int bytes_read = vfs_read(fd, buffer, MAX_ELF_SIZE);
    vfs_close(fd);

    if (bytes_read <= 0) {
        kfree(buffer);
        return -4;
    }

    /* Carrega o ELF do buffer */
    int result = load_elf(buffer, (u32)bytes_read, ctx);
    kfree(buffer);

    return result;
}
