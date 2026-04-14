/*
 * syscall.c - Implementação das chamadas de sistema
 * Descrição: Interface de syscalls entre user mode e kernel, usando
 *            a interrupção 0x80 (estilo Linux). Registra os handlers
 *            e despacha as chamadas para as funções correspondentes.
 */

#include "../include/syscall.h"
#include "../include/process.h"
#include "../include/vfs.h"
#include "../include/memory.h"
#include "../include/scheduler.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_putchar(char c);

/* ============================================================
 * TABELA DE SYSCALLS
 * ============================================================ */
static syscall_handler_t syscall_table[SYSCALL_COUNT];

/* ============================================================
 * IMPLEMENTAÇÕES DAS SYSCALLS
 * ============================================================ */

/*
 * Função: sys_exit
 * Descrição: Termina o processo atual com o código de saída fornecido.
 *            Libera todos os recursos associados ao processo.
 */
u32 sys_exit(u32 exit_code, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);

    process_t *proc = get_current_process();
    if (proc) {
        terminate_process(proc->pid, (s32)exit_code);
    }
    return 0;
}

/*
 * Função: sys_read
 * Descrição: Lê até 'size' bytes do descritor de arquivo 'fd'
 *            para o buffer em 'buf_ptr'. Retorna bytes lidos.
 */
u32 sys_read(u32 fd, u32 buf_ptr, u32 size, u32 a4, u32 a5) {
    UNUSED(a4); UNUSED(a5);

    void *buffer = (void *)buf_ptr;
    if (!buffer) return (u32)-1;

    /* fd=0 é stdin - leitura do teclado (simplificado) */
    if (fd == 0) {
        extern char kb_getchar(void);
        if (size > 0) {
            char *buf = (char *)buffer;
            *buf = kb_getchar();
            return 1;
        }
        return 0;
    }

    int result = vfs_read((int)fd, buffer, size);
    return (result < 0) ? (u32)-1 : (u32)result;
}

/*
 * Função: sys_write
 * Descrição: Escreve 'size' bytes do buffer 'buf_ptr' no descritor
 *            de arquivo 'fd'. fd=1 é stdout, fd=2 é stderr.
 */
u32 sys_write(u32 fd, u32 buf_ptr, u32 size, u32 a4, u32 a5) {
    UNUSED(a4); UNUSED(a5);

    const char *buffer = (const char *)buf_ptr;
    if (!buffer) return (u32)-1;

    /* fd=1 (stdout) ou fd=2 (stderr): escreve na tela */
    if (fd == 1 || fd == 2) {
        for (u32 i = 0; i < size && buffer[i]; i++) {
            screen_putchar(buffer[i]);
        }
        return size;
    }

    int result = vfs_write((int)fd, buffer, size);
    return (result < 0) ? (u32)-1 : (u32)result;
}

/*
 * Função: sys_open
 * Descrição: Abre o arquivo no caminho 'path_ptr' com as flags
 *            especificadas. Retorna o fd ou -1 em erro.
 */
u32 sys_open(u32 path_ptr, u32 flags, u32 a3, u32 a4, u32 a5) {
    UNUSED(a3); UNUSED(a4); UNUSED(a5);

    const char *path = (const char *)path_ptr;
    if (!path) return (u32)-1;

    int fd = vfs_open(path, flags);
    return (fd < 0) ? (u32)-1 : (u32)fd;
}

/*
 * Função: sys_close
 * Descrição: Fecha o descritor de arquivo especificado.
 *            Retorna 0 em sucesso ou -1 em erro.
 */
u32 sys_close(u32 fd, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);

    if (fd <= 2) return (u32)-1; /* Não fecha stdin/stdout/stderr */
    vfs_close((int)fd);
    return 0;
}

/*
 * Função: sys_exec
 * Descrição: Carrega e executa um programa a partir do caminho especificado.
 */
u32 sys_exec(u32 path_ptr, u32 argv_ptr, u32 envp_ptr, u32 a4, u32 a5) {
    UNUSED(argv_ptr); UNUSED(envp_ptr); UNUSED(a4); UNUSED(a5);

    const char *path = (const char *)path_ptr;
    if (!path) return (u32)-1;

    extern int exec_program(const char *path);
    int result = exec_program(path);
    return (result < 0) ? (u32)-1 : (u32)result;
}

/*
 * Função: sys_getpid
 * Descrição: Retorna o PID do processo atual.
 */
u32 sys_getpid(u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a1); UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);

    process_t *proc = get_current_process();
    return proc ? proc->pid : 0;
}

/*
 * Função: sys_sleep
 * Descrição: Coloca o processo atual para dormir pelo número de ms.
 */
static u32 sys_sleep_impl(u32 ms, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);
    thread_sleep(ms);
    return 0;
}

/*
 * Função: sys_malloc_impl
 * Descrição: Aloca memória para o processo em user mode.
 */
static u32 sys_malloc_impl(u32 size, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);
    return (u32)kmalloc(size);
}

/*
 * Função: sys_free_impl
 * Descrição: Libera memória alocada pelo processo em user mode.
 */
static u32 sys_free_impl(u32 ptr, u32 a2, u32 a3, u32 a4, u32 a5) {
    UNUSED(a2); UNUSED(a3); UNUSED(a4); UNUSED(a5);
    kfree((void *)ptr);
    return 0;
}

/* ============================================================
 * INICIALIZAÇÃO E DISPATCH
 * ============================================================ */

/*
 * Função: init_syscalls
 * Descrição: Inicializa a tabela de syscalls e registra o handler
 *            da interrupção 0x80 no IDT.
 */
void init_syscalls(void) {
    /* Zera a tabela */
    for (int i = 0; i < SYSCALL_COUNT; i++) {
        syscall_table[i] = NULL;
    }

    /* Registra syscalls na tabela */
    syscall_table[SYS_EXIT]   = sys_exit;
    syscall_table[SYS_READ]   = sys_read;
    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_OPEN]   = sys_open;
    syscall_table[SYS_CLOSE]  = sys_close;
    syscall_table[SYS_EXEC]   = sys_exec;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_SLEEP]  = sys_sleep_impl;
    syscall_table[SYS_MALLOC] = sys_malloc_impl;
    syscall_table[SYS_FREE]   = sys_free_impl;

    /*
     * Em um kernel completo, aqui registraríamos o handler no IDT:
     *   idt_set_gate(SYSCALL_INT, syscall_entry, KERNEL_CS, IDT_USER_INT);
     *
     * O handler de interrupção salvaria os registradores e chamaria
     * syscall_handler() com o contexto adequado.
     */
}

/*
 * Função: syscall_handler
 * Descrição: Handler principal da interrupção 0x80. Despacha a chamada
 *            para a função correspondente na tabela de syscalls.
 *            Chamado pelo handler de interrupção em Assembly.
 */
void syscall_handler(syscall_regs_t *regs) {
    if (!regs) return;

    u32 num = regs->syscall_num;

    if (num >= SYSCALL_COUNT || syscall_table[num] == NULL) {
        /* Syscall inválida */
        regs->ret = (u32)-1;
        return;
    }

    /* Despacha para o handler correto */
    regs->ret = syscall_table[num](
        regs->arg1,
        regs->arg2,
        regs->arg3,
        regs->arg4,
        regs->arg5
    );
}
