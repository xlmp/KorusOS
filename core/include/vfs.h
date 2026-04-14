#ifndef VFS_H
#define VFS_H

/*
 * vfs.h - Cabeçalho do Sistema de Arquivos Virtual (VFS)
 * Descrição: Define estruturas e protótipos do VFS, inspirado no modelo
 *            do kernel Linux, para abstração de sistemas de arquivos.
 */

#include "kernel.h"

/* ============================================================
 * CONSTANTES VFS
 * ============================================================ */
#define VFS_MAX_MOUNTS      8           /* Máximo de sistemas montados */
#define VFS_MAX_OPEN_FILES  256         /* Máximo de arquivos abertos */
#define VFS_NAME_LEN        256         /* Tamanho máximo de nome */
#define VFS_PATH_LEN        1024        /* Tamanho máximo de caminho */

/* Flags de abertura de arquivo */
#define VFS_O_READ          (1 << 0)    /* Abrir para leitura */
#define VFS_O_WRITE         (1 << 1)    /* Abrir para escrita */
#define VFS_O_CREATE        (1 << 2)    /* Criar se não existir */
#define VFS_O_APPEND        (1 << 3)    /* Escrever no final */
#define VFS_O_TRUNC         (1 << 4)    /* Truncar ao abrir */

/* Tipos de nó VFS */
typedef enum {
    VFS_FILE        = 0x01,
    VFS_DIRECTORY   = 0x02,
    VFS_CHARDEV     = 0x03,
    VFS_BLOCKDEV    = 0x04,
    VFS_PIPE        = 0x05,
    VFS_SYMLINK     = 0x06,
    VFS_MOUNTPOINT  = 0x07,
} vfs_node_type_t;

/* ============================================================
 * ESTRUTURAS VFS
 * ============================================================ */

/* Forward declarations */
struct vfs_node;
struct vfs_dirent;

/* Operações de um nó (similar ao file_operations do Linux) */
typedef struct {
    u32  (*read)(struct vfs_node *node, u32 offset, u32 size, u8 *buffer);
    u32  (*write)(struct vfs_node *node, u32 offset, u32 size, u8 *buffer);
    bool (*open)(struct vfs_node *node, u32 flags);
    void (*close)(struct vfs_node *node);
    struct vfs_dirent *(*readdir)(struct vfs_node *node, u32 index);
    struct vfs_node   *(*finddir)(struct vfs_node *node, const char *name);
} vfs_ops_t;

/* Nó VFS (similar ao inode do Linux) */
typedef struct vfs_node {
    char            name[VFS_NAME_LEN]; /* Nome do arquivo/diretório */
    u32             inode;              /* Número de inode */
    u32             size;               /* Tamanho em bytes */
    vfs_node_type_t type;               /* Tipo do nó */
    u32             flags;              /* Flags do nó */
    u32             uid, gid;           /* Dono e grupo */
    u32             atime, mtime, ctime;/* Timestamps */
    vfs_ops_t      *ops;                /* Operações do nó */
    struct vfs_node *mount_point;       /* Ponto de montagem (se houver) */
} vfs_node_t;

/* Entrada de diretório */
typedef struct vfs_dirent {
    char    name[VFS_NAME_LEN]; /* Nome da entrada */
    u32     inode;              /* Inode correspondente */
} vfs_dirent_t;

/* Descritor de arquivo aberto */
typedef struct {
    vfs_node_t *node;       /* Nó VFS associado */
    u32         offset;     /* Posição atual de leitura/escrita */
    u32         flags;      /* Flags de abertura */
    bool        in_use;     /* Descritor em uso */
} file_descriptor_t;

/* Ponto de montagem */
typedef struct {
    char        path[VFS_PATH_LEN]; /* Caminho do ponto de montagem */
    vfs_node_t *root;               /* Nó raiz do sistema montado */
    bool        in_use;             /* Ponto de montagem em uso */
    char        fs_type[16];        /* Tipo de sistema de arquivos */
} vfs_mount_t;

/* ============================================================
 * PROTÓTIPOS VFS
 * ============================================================ */

/*
 * Função: vfs_init
 * Descrição: Inicializa o Sistema de Arquivos Virtual, criando
 *            a raiz "/" e preparando as estruturas internas.
 */
void vfs_init(void);

/*
 * Função: vfs_mount
 * Descrição: Monta um sistema de arquivos no caminho especificado.
 *            O nó raiz é associado ao ponto de montagem.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int vfs_mount(const char *path, vfs_node_t *root, const char *fs_type);

/*
 * Função: vfs_open
 * Descrição: Abre um arquivo no caminho especificado com as flags informadas.
 *            Retorna o descritor de arquivo (fd) ou -1 em erro.
 */
int vfs_open(const char *path, u32 flags);

/*
 * Função: vfs_read
 * Descrição: Lê dados de um arquivo aberto pelo descritor fd,
 *            armazenando no buffer fornecido. Retorna bytes lidos.
 */
int vfs_read(int fd, void *buffer, u32 size);

/*
 * Função: vfs_write
 * Descrição: Escreve dados em um arquivo aberto pelo descritor fd.
 *            Retorna bytes escritos ou erro negativo.
 */
int vfs_write(int fd, const void *buffer, u32 size);

/*
 * Função: vfs_close
 * Descrição: Fecha o arquivo associado ao descritor fd,
 *            liberando o slot de descritor.
 */
void vfs_close(int fd);

/*
 * Função: vfs_list
 * Descrição: Lista o conteúdo do diretório especificado,
 *            exibindo os nomes dos arquivos e subdiretórios.
 */
void vfs_list(const char *path);

/*
 * Função: vfs_resolve_path
 * Descrição: Resolve um caminho absoluto e retorna o nó VFS
 *            correspondente. Retorna NULL se não encontrado.
 */
vfs_node_t *vfs_resolve_path(const char *path);

#endif /* VFS_H */
