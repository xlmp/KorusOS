#ifndef SYSCALL_H
#define SYSCALL_H

/*
 * syscall.h - Cabeçalho das chamadas de sistema (syscalls)
 * Descrição: Define números, estruturas e protótipos da interface de
 *            chamadas de sistema entre o modo usuário e o kernel.
 */

#include "kernel.h"

/* ============================================================
 * NÚMEROS DE SYSCALLS (tabela de chamadas de sistema)
 * ============================================================ */
#define SYS_EXIT        0   /* Terminar processo */
#define SYS_READ        1   /* Ler de descritor de arquivo */
#define SYS_WRITE       2   /* Escrever em descritor de arquivo */
#define SYS_OPEN        3   /* Abrir arquivo */
#define SYS_CLOSE       4   /* Fechar arquivo */
#define SYS_EXEC        5   /* Executar programa */
#define SYS_FORK        6   /* Clonar processo */
#define SYS_GETPID      7   /* Obter PID do processo atual */
#define SYS_SLEEP       8   /* Dormir por N milissegundos */
#define SYS_MALLOC      9   /* Alocar memória em user mode */
#define SYS_FREE        10  /* Liberar memória em user mode */
#define SYS_STAT        11  /* Obter informações de arquivo */
#define SYS_MKDIR       12  /* Criar diretório */
#define SYS_REMOVE      13  /* Remover arquivo */
#define SYS_READDIR     14  /* Ler entradas de diretório */
#define SYS_GETTIME     15  /* Obter tempo atual do sistema */
#define SYSCALL_COUNT   16  /* Total de syscalls implementadas */

/* Interrupção de syscall (Linux-like: int 0x80) */
#define SYSCALL_INT     0x80

/* ============================================================
 * REGISTROS DE CONTEXTO DA SYSCALL
 * ============================================================ */
typedef struct {
    uptr syscall_num;    /* Número da syscall (em rax) */
    uptr arg1;           /* Primeiro argumento (rdi) */
    uptr arg2;           /* Segundo argumento (rsi) */
    uptr arg3;           /* Terceiro argumento (rdx) */
    uptr arg4;           /* Quarto argumento (r10) */
    uptr arg5;           /* Quinto argumento (r8) */
    uptr ret;            /* Valor de retorno (rax) */
} syscall_regs_t;

/* Tipo de ponteiro para handler de syscall */
typedef uptr (*syscall_handler_t)(uptr arg1, uptr arg2, uptr arg3,
                                  uptr arg4, uptr arg5);

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: init_syscalls
 * Descrição: Inicializa a tabela de syscalls e registra o handler
 *            da interrupção 0x80 no IDT (Interrupt Descriptor Table).
 */
void init_syscalls(void);

/*
 * Função: syscall_handler
 * Descrição: Handler principal da interrupção 0x80. Despacha a chamada
 *            para a função correspondente na tabela de syscalls.
 */
void syscall_handler(syscall_regs_t *regs);

/* ============================================================
 * IMPLEMENTAÇÕES DE SYSCALLS
 * ============================================================ */

/*
 * Função: sys_exit
 * Descrição: Termina o processo atual com o código de saída fornecido.
 *            Libera todos os recursos associados ao processo.
 */
u32 sys_exit(u32 exit_code, u32 a2, u32 a3, u32 a4, u32 a5);

/*
 * Função: sys_read
 * Descrição: Lê até 'size' bytes do descritor de arquivo 'fd'
 *            para o buffer em 'buf_ptr'. Retorna bytes lidos.
 */
u32 sys_read(u32 fd, u32 buf_ptr, u32 size, u32 a4, u32 a5);

/*
 * Função: sys_write
 * Descrição: Escreve 'size' bytes do buffer 'buf_ptr' no descritor
 *            de arquivo 'fd'. fd=1 é stdout, fd=2 é stderr.
 */
u32 sys_write(u32 fd, u32 buf_ptr, u32 size, u32 a4, u32 a5);

/*
 * Função: sys_open
 * Descrição: Abre o arquivo no caminho 'path_ptr' com as flags
 *            especificadas. Retorna o fd ou -1 em erro.
 */
u32 sys_open(u32 path_ptr, u32 flags, u32 a3, u32 a4, u32 a5);

/*
 * Função: sys_close
 * Descrição: Fecha o descritor de arquivo especificado.
 *            Retorna 0 em sucesso ou -1 em erro.
 */
u32 sys_close(u32 fd, u32 a2, u32 a3, u32 a4, u32 a5);

/*
 * Função: sys_exec
 * Descrição: Substitui a imagem do processo atual pelo programa
 *            no caminho especificado (equivalente ao execve do Linux).
 */
u32 sys_exec(u32 path_ptr, u32 argv_ptr, u32 envp_ptr, u32 a4, u32 a5);

/*
 * Função: sys_getpid
 * Descrição: Retorna o PID (Process ID) do processo atual.
 */
u32 sys_getpid(u32 a1, u32 a2, u32 a3, u32 a4, u32 a5);

#endif /* SYSCALL_H */
