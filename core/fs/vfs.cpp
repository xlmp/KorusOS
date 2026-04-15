/*
 * vfs.c - Implementação do Sistema de Arquivos Virtual (VFS)
 * Descrição: VFS inspirado no modelo do kernel Linux, fornecendo
 *            uma camada de abstração sobre sistemas de arquivos reais.
 *            Suporta montagem, abertura, leitura, escrita e listagem.
 */

#include "../include/vfs.h"
#include "../include/memory.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);

/* ============================================================
 * VARIÁVEIS INTERNAS DO VFS
 * ============================================================ */
static vfs_mount_t      mounts[VFS_MAX_MOUNTS];
static file_descriptor_t fds[VFS_MAX_OPEN_FILES];
static vfs_node_t        vfs_root;
static u32               inode_counter = 1;

/* ============================================================
 * PSEUDO-ARQUIVOS INTERNOS (VFS em memória)
 * ============================================================ */

/* Buffer de leitura/escrita para arquivos em memória */
#define MEMFS_MAX_FILES     32
#define MEMFS_MAX_FILESIZE  4096

typedef struct {
    char    name[VFS_NAME_LEN];
    u8      data[MEMFS_MAX_FILESIZE];
    u32     size;
    bool    used;
    u32     inode;
} memfs_file_t;

static memfs_file_t memfs_files[MEMFS_MAX_FILES];
static u32 memfs_file_count = 0;

/* ============================================================
 * OPERAÇÕES DO SISTEMA DE ARQUIVOS EM MEMÓRIA (memfs)
 * ============================================================ */

/*
 * Função: memfs_read
 * Descrição: Lê dados de um arquivo em memória do sistema de arquivos virtual.
 */
static u32 memfs_read(vfs_node_t *node, u32 offset, u32 size, u8 *buffer) {
    /* Encontra o arquivo pelo inode */
    for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
        if (memfs_files[i].used && memfs_files[i].inode == node->inode) {
            if (offset >= memfs_files[i].size) return 0;
            u32 bytes = MIN(size, memfs_files[i].size - offset);
            kmemcpy(buffer, memfs_files[i].data + offset, bytes);
            return bytes;
        }
    }
    return 0;
}

/*
 * Função: memfs_write
 * Descrição: Escreve dados em um arquivo em memória do sistema de arquivos virtual.
 */
static u32 memfs_write(vfs_node_t *node, u32 offset, u32 size, u8 *buffer) {
    for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
        if (memfs_files[i].used && memfs_files[i].inode == node->inode) {
            u32 end = offset + size;
            if (end > MEMFS_MAX_FILESIZE) {
                size = MEMFS_MAX_FILESIZE - offset;
                end  = MEMFS_MAX_FILESIZE;
            }
            kmemcpy(memfs_files[i].data + offset, buffer, size);
            if (end > memfs_files[i].size) {
                memfs_files[i].size = end;
                node->size          = end;
            }
            return size;
        }
    }
    return 0;
}

/*
 * Função: memfs_open
 * Descrição: Abre um arquivo no sistema de arquivos em memória.
 */
static bool memfs_open(vfs_node_t *node, u32 flags) {
    UNUSED(flags);
    UNUSED(node);
    return TRUE;
}

/*
 * Função: memfs_close
 * Descrição: Fecha um arquivo no sistema de arquivos em memória.
 */
static void memfs_close(vfs_node_t *node) {
    UNUSED(node);
}

/*
 * Função: memfs_readdir
 * Descrição: Lê uma entrada do diretório raiz do sistema de arquivos em memória.
 */
static vfs_dirent_t *memfs_readdir(vfs_node_t *node, u32 index) {
    UNUSED(node);
    static vfs_dirent_t dirent;
    u32 found = 0;

    for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
        if (memfs_files[i].used) {
            if (found == index) {
                u32 j = 0;
                while (memfs_files[i].name[j] && j < VFS_NAME_LEN - 1) {
                    dirent.name[j] = memfs_files[i].name[j];
                    j++;
                }
                dirent.name[j] = '\0';
                dirent.inode   = memfs_files[i].inode;
                return &dirent;
            }
            found++;
        }
    }
    return (vfs_dirent_t *)NULL;
}

/*
 * Função: memfs_finddir
 * Descrição: Encontra um arquivo pelo nome no sistema de arquivos em memória.
 */
static vfs_node_t *memfs_finddir(vfs_node_t *node, const char *name) {
    UNUSED(node);
    static vfs_node_t found_node;
    static vfs_ops_t  memfs_ops;

    for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
        if (!memfs_files[i].used) continue;

        /* Compara nome */
        u32 j = 0;
        while (memfs_files[i].name[j] && name[j] &&
               memfs_files[i].name[j] == name[j]) j++;

        if (!memfs_files[i].name[j] && !name[j]) {
            /* Preenche o nó encontrado */
            u32 k = 0;
            while (memfs_files[i].name[k] && k < VFS_NAME_LEN - 1) {
                found_node.name[k] = memfs_files[i].name[k];
                k++;
            }
            found_node.name[k]  = '\0';
            found_node.inode    = memfs_files[i].inode;
            found_node.size     = memfs_files[i].size;
            found_node.type     = VFS_FILE;

            memfs_ops.read    = memfs_read;
            memfs_ops.write   = memfs_write;
            memfs_ops.open    = memfs_open;
            memfs_ops.close   = memfs_close;
            memfs_ops.readdir = memfs_readdir;
            memfs_ops.finddir = memfs_finddir;

            found_node.ops = &memfs_ops;
            return &found_node;
        }
    }
    return (vfs_node_t*)NULL;
}

/* Operações do root do memfs */
static vfs_ops_t root_ops = {
    .read    = (u32 (*)(struct vfs_node *, u32, u32, u8 *))NULL,
    .write   = (u32 (*)(struct vfs_node *, u32, u32, u8 *))NULL,
    .open    = memfs_open,
    .close   = memfs_close,
    .readdir = memfs_readdir,
    .finddir = memfs_finddir,
};

/* ============================================================
 * FUNÇÕES AUXILIARES
 * ============================================================ */

/*
 * Função: kstrcmp
 * Descrição: Compara duas strings. Retorna 0 se iguais.
 */
static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(*a) - (int)(*b);
}

/*
 * Função: find_free_fd
 * Descrição: Encontra um descritor de arquivo livre. Retorna -1 se não há.
 */
static int find_free_fd(void) {
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!fds[i].in_use) return i;
    }
    return -1;
}

/* ============================================================
 * FUNÇÕES PÚBLICAS DO VFS
 * ============================================================ */

/*
 * Função: vfs_init
 * Descrição: Inicializa o Sistema de Arquivos Virtual, criando
 *            a raiz "/" e preparando as estruturas internas.
 */
void vfs_init(void) {
    /* Inicializa estruturas internas */
    kmemset(mounts,     0, sizeof(mounts));
    kmemset(fds,        0, sizeof(fds));
    kmemset(memfs_files, 0, sizeof(memfs_files));
    memfs_file_count = 0;
    inode_counter    = 1;

    /* Configura o nó raiz */
    vfs_root.inode = 0;
    vfs_root.type  = VFS_DIRECTORY;
    vfs_root.size  = 0;
    vfs_root.ops   = &root_ops;
    vfs_root.name[0] = '/';
    vfs_root.name[1] = '\0';

    /* Cria alguns arquivos padrão do sistema */
    const char *readme = "Korus OS - Sistema Operacional Experimental\nVersao 0.1.0\n";
    for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
        if (!memfs_files[i].used) {
            memfs_files[i].used  = TRUE;
            memfs_files[i].inode = inode_counter++;
            /* Copia "README.TXT" */
            const char *fn = "README.TXT";
            u32 j = 0;
            while (fn[j] && j < VFS_NAME_LEN - 1) {
                memfs_files[i].name[j] = fn[j];
                j++;
            }
            memfs_files[i].name[j] = '\0';

            /* Copia conteúdo */
            u32 k = 0;
            while (readme[k] && k < MEMFS_MAX_FILESIZE - 1) {
                memfs_files[i].data[k] = (u8)readme[k];
                k++;
            }
            memfs_files[i].size = k;
            memfs_file_count++;
            break;
        }
    }

    /* Monta o sistema de arquivos raiz */
    vfs_mount("/", &vfs_root, "memfs");
}

/*
 * Função: vfs_mount
 * Descrição: Monta um sistema de arquivos no caminho especificado.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int vfs_mount(const char *path, vfs_node_t *root, const char *fs_type) {
    for (u32 i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].in_use) {
            u32 j = 0;
            while (path[j] && j < VFS_PATH_LEN - 1) {
                mounts[i].path[j] = path[j];
                j++;
            }
            mounts[i].path[j] = '\0';
            mounts[i].root    = root;
            mounts[i].in_use  = TRUE;

            j = 0;
            while (fs_type[j] && j < 15) {
                mounts[i].fs_type[j] = fs_type[j];
                j++;
            }
            mounts[i].fs_type[j] = '\0';
            return 0;
        }
    }
    return -1; /* Sem slot de montagem disponível */
}

/*
 * Função: vfs_resolve_path
 * Descrição: Resolve um caminho absoluto e retorna o nó VFS correspondente.
 *            Retorna NULL se não encontrado.
 */
vfs_node_t *vfs_resolve_path(const char *path) {
    if (!path || path[0] != '/') return (vfs_node_t *)NULL;

    /* Para a raiz "/" */
    if (path[1] == '\0') return &vfs_root;

    /* Navega pelo caminho (somente nível único por simplicidade) */
    const char *filename = path + 1;
    /* Remove barra inicial adicional */
    while (*filename == '/') filename++;

    if (vfs_root.ops && vfs_root.ops->finddir) {
        return vfs_root.ops->finddir(&vfs_root, filename);
    }

    return (vfs_node_t*)NULL;
}

/*
 * Função: vfs_open
 * Descrição: Abre um arquivo no caminho especificado com as flags informadas.
 *            Retorna o descritor de arquivo (fd) ou -1 em erro.
 */
int vfs_open(const char *path, u32 flags) {
    vfs_node_t *node = vfs_resolve_path(path);

    /* Cria arquivo se flag CREATE e não existe */
    if (!node && (flags & VFS_O_CREATE)) {
        for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
            if (!memfs_files[i].used) {
                memfs_files[i].used  = TRUE;
                memfs_files[i].inode = inode_counter++;
                memfs_files[i].size  = 0;

                const char *fn = path + 1; /* Remove '/' inicial */
                u32 j = 0;
                while (fn[j] && j < VFS_NAME_LEN - 1) {
                    memfs_files[i].name[j] = fn[j];
                    j++;
                }
                memfs_files[i].name[j] = '\0';
                memfs_file_count++;

                node = memfs_finddir(&vfs_root, fn);
                break;
            }
        }
    }

    if (!node) return -1;

    int fd = find_free_fd();
    if (fd < 0) return -1;

    if (node->ops && node->ops->open) {
        if (!node->ops->open(node, flags)) return -1;
    }

    fds[fd].node   = node;
    fds[fd].offset = 0;
    fds[fd].flags  = flags;
    fds[fd].in_use = TRUE;

    return fd;
}

/*
 * Função: vfs_read
 * Descrição: Lê dados de um arquivo aberto. Retorna bytes lidos.
 */
int vfs_read(int fd, void *buffer, u32 size) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !fds[fd].in_use) return -1;

    file_descriptor_t *f = &fds[fd];
    if (!f->node || !f->node->ops || !f->node->ops->read) return -1;

    u32 bytes = f->node->ops->read(f->node, f->offset, size, (u8 *)buffer);
    f->offset += bytes;
    return (int)bytes;
}

/*
 * Função: vfs_write
 * Descrição: Escreve dados em um arquivo aberto. Retorna bytes escritos.
 */
int vfs_write(int fd, const void *buffer, u32 size) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !fds[fd].in_use) return -1;

    file_descriptor_t *f = &fds[fd];
    if (!f->node || !f->node->ops || !f->node->ops->write) return -1;

    u32 bytes = f->node->ops->write(f->node, f->offset, size, (u8 *)buffer);
    f->offset += bytes;
    return (int)bytes;
}

/*
 * Função: vfs_close
 * Descrição: Fecha o arquivo associado ao descritor fd.
 */
void vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !fds[fd].in_use) return;

    file_descriptor_t *f = &fds[fd];
    if (f->node && f->node->ops && f->node->ops->close) {
        f->node->ops->close(f->node);
    }

    f->in_use = FALSE;
    f->node   = (vfs_node_t *)NULL;
    f->offset = 0;
}

/*
 * Função: vfs_list
 * Descrição: Lista o conteúdo do diretório especificado.
 */
void vfs_list(const char *path) {
    UNUSED(path);

    screen_println("=== Conteudo do Diretorio ===");
    screen_println("  Nome               Tamanho   Tipo");
    screen_println("  -----------------  --------  ----");

    u32 index = 0;
    while (TRUE) {
        vfs_dirent_t *dirent = vfs_root.ops->readdir(&vfs_root, index++);
        if (!dirent) break;

        screen_print("  ");
        screen_print(dirent->name);
        screen_print("    ");

        /* Exibe tamanho */
        for (u32 i = 0; i < MEMFS_MAX_FILES; i++) {
            if (memfs_files[i].used && memfs_files[i].inode == dirent->inode) {
                screen_print_int((s32)memfs_files[i].size);
                screen_print(" bytes");
                break;
            }
        }
        screen_println("  [ARQUIVO]");
    }

    screen_print("Total: ");
    screen_print_int((s32)memfs_file_count);
    screen_println(" arquivo(s)");
}
