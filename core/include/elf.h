#ifndef ELF_H
#define ELF_H

/*
 * elf.h - Cabeçalho do carregador de executáveis ELF
 * Descrição: Define estruturas do formato ELF (Executable and Linkable
 *            Format) e protótipos do carregador de programas do kernel.
 */

#include "kernel.h"

/* ============================================================
 * CONSTANTES ELF
 * ============================================================ */
#define ELF_MAGIC           0x464C457F  /* "\x7FELF" em little-endian */
#define ELF_CLASS_32        1           /* Arquivo ELF de 32 bits */
#define ELF_CLASS_64        2           /* Arquivo ELF de 64 bits */
#define ELF_DATA_LSB        1           /* Little-endian */
#define ELF_TYPE_EXEC       2           /* Executável */
#define ELF_TYPE_DYN        3           /* Biblioteca compartilhada */
#define ELF_ARCH_386        3           /* Arquitetura x86 */
#define ELF_ARCH_X86_64     62          /* Arquitetura x86_64 */
#define ELF_PHDR_LOAD       1           /* Segmento carregável */
#define ELF_VERSION         1           /* Versão ELF atual */

/* ============================================================
 * ESTRUTURA ELF32 (cabeçalho principal)
 * ============================================================ */
typedef struct {
    u8  e_ident[16];    /* Identificador ELF (magic + classe + dados...) */
    u16 e_type;         /* Tipo do arquivo (exec, dyn, rel...) */
    u16 e_machine;      /* Arquitetura alvo */
    u32 e_version;      /* Versão do ELF */
    u32 e_entry;        /* Endereço de entrada (ponto de início) */
    u32 e_phoff;        /* Offset do Program Header Table */
    u32 e_shoff;        /* Offset do Section Header Table */
    u32 e_flags;        /* Flags específicas da arquitetura */
    u16 e_ehsize;       /* Tamanho do cabeçalho ELF */
    u16 e_phentsize;    /* Tamanho de cada entrada do Program Header */
    u16 e_phnum;        /* Número de entradas no Program Header */
    u16 e_shentsize;    /* Tamanho de cada entrada do Section Header */
    u16 e_shnum;        /* Número de entradas no Section Header */
    u16 e_shstrndx;     /* Índice da seção de nomes de seções */
} PACKED elf32_header_t;

/* ============================================================
 * PROGRAM HEADER ELF32 (segmentos carregáveis)
 * ============================================================ */
typedef struct {
    u32 p_type;         /* Tipo do segmento (LOAD, DYNAMIC, etc.) */
    u32 p_offset;       /* Offset do segmento no arquivo */
    u32 p_vaddr;        /* Endereço virtual de destino */
    u32 p_paddr;        /* Endereço físico (geralmente igual ao virtual) */
    u32 p_filesz;       /* Tamanho do segmento no arquivo */
    u32 p_memsz;        /* Tamanho do segmento na memória */
    u32 p_flags;        /* Flags de permissão (R/W/X) */
    u32 p_align;        /* Alinhamento do segmento */
} PACKED elf32_phdr_t;

/* ============================================================
 * SECTION HEADER ELF32
 * ============================================================ */
typedef struct {
    u32 sh_name;        /* Índice do nome na seção de strings */
    u32 sh_type;        /* Tipo da seção */
    u32 sh_flags;       /* Flags da seção */
    u32 sh_addr;        /* Endereço virtual */
    u32 sh_offset;      /* Offset no arquivo */
    u32 sh_size;        /* Tamanho da seção */
    u32 sh_link;        /* Link para outra seção */
    u32 sh_info;        /* Informação adicional */
    u32 sh_addralign;   /* Alinhamento */
    u32 sh_entsize;     /* Tamanho de entradas (se tabela) */
} PACKED elf32_shdr_t;

/* ============================================================
 * ESTRUTURA DE CONTEXTO DO ELF CARREGADO
 * ============================================================ */
typedef struct {
    u32 entry_point;    /* Endereço de início da execução */
    u32 load_base;      /* Endereço base de carregamento */
    u32 load_end;       /* Endereço final do segmento carregado */
    bool valid;         /* ELF foi validado e carregado com sucesso */
} elf_context_t;

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: validate_elf
 * Descrição: Verifica se o buffer fornecido contém um ELF válido,
 *            checando o magic number, classe, tipo e arquitetura.
 *            Retorna true se válido, false caso contrário.
 */
bool validate_elf(const u8 *data, u32 size);

/*
 * Função: load_elf
 * Descrição: Carrega um executável ELF presente no buffer de dados
 *            para a memória virtual, mapeando todos os segmentos LOAD.
 *            Preenche o contexto com endereço de entrada e informações.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int load_elf(const u8 *data, u32 size, elf_context_t *ctx);

/*
 * Função: map_elf_segments
 * Descrição: Percorre os Program Headers do ELF e mapeia cada segmento
 *            do tipo LOAD na memória virtual do processo.
 *            Respeita as flags de permissão de cada segmento.
 */
int map_elf_segments(const elf32_header_t *hdr, const u8 *data);

/*
 * Função: load_elf_from_file
 * Descrição: Carrega um executável ELF a partir de um caminho no VFS.
 *            Lê o arquivo, valida e carrega o binário.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int load_elf_from_file(const char *path, elf_context_t *ctx);

#endif /* ELF_H */
