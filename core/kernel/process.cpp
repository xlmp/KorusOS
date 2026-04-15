/*
 * process.c - Gerenciamento de processos
 * Descrição: Implementação do gerenciador de processos, incluindo criação,
 *            término, alternância entre kernel mode e user mode, e listagem.
 */

#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/memory.h"
#include "../include/elf.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(u32 val);

/* ============================================================
 * VARIÁVEIS INTERNAS
 * ============================================================ */
static process_t  proc_table[MAX_PROCESSES];
static u32        next_pid      = 1;
static u32        proc_count    = 0;
static process_t *current_proc  = (process_t *)NULL;

/* ============================================================
 * FUNÇÕES AUXILIARES
 * ============================================================ */

/*
 * Função: kstrncpy_proc
 * Descrição: Copia string com limite de tamanho para uso interno.
 */
static void kstrncpy_proc(char *dest, const char *src, u32 max) {
    u32 i = 0;
    while (i < max - 1 && src && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/*
 * Função: find_free_proc_slot
 * Descrição: Encontra um slot livre na tabela de processos.
 *            Retorna o índice ou -1 se a tabela estiver cheia.
 */
static int find_free_proc_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_EMPTY) return i;
    }
    return -1;
}

/* ============================================================
 * FUNÇÕES PÚBLICAS DE PROCESSOS
 * ============================================================ */

/*
 * Função: init_processes
 * Descrição: Inicializa o subsistema de gerenciamento de processos,
 *            configurando a tabela de processos e o processo inicial.
 */
void init_processes(void) {
    kmemset(proc_table, 0, sizeof(proc_table));
    next_pid     = 1;
    proc_count   = 0;
    current_proc = (process_t *)NULL;

    /* Cria o processo kernel (PID 0) */
    proc_table[0].pid       = 0;
    proc_table[0].state     = PROC_RUNNING;
    proc_table[0].privilege = PRIVILEGE_KERNEL;
    proc_table[0].parent_pid = 0;
    kstrncpy_proc(proc_table[0].name, "kernel", PROC_NAME_LEN);
    current_proc = &proc_table[0];
    proc_count   = 1;
}

/*
 * Função: create_process
 * Descrição: Cria um novo processo com o nome e ponto de entrada especificados.
 *            Aloca espaço de endereçamento próprio e configura as estruturas.
 *            Retorna o PID ou -1 em caso de erro.
 */
int create_process(const char *name, u32 entry_point, privilege_t privilege) {
    if (proc_count >= MAX_PROCESSES) return -1;

    int slot = find_free_proc_slot();
    if (slot < 0) return -1;

    process_t *proc = &proc_table[slot];
    kmemset(proc, 0, sizeof(process_t));

    proc->pid         = next_pid++;
    proc->state       = PROC_RUNNING;
    proc->privilege   = privilege;
    proc->entry_point = entry_point;
    proc->parent_pid  = current_proc ? current_proc->pid : 0;
    proc->heap_base   = USER_HEAP_BASE;
    proc->heap_size   = 0;
    kstrncpy_proc(proc->name, name ? name : "processo", PROC_NAME_LEN);

    /* Aloca pilha em user mode */
    proc->user_stack = 0;
    if (privilege == PRIVILEGE_USER) {
        u8 *stack_mem = (u8 *)kmalloc(USER_STACK_SIZE);
        if (!stack_mem) return -1;
        proc->user_stack = (u32)stack_mem + USER_STACK_SIZE;
    }

    /* Cria thread principal do processo */
    char thread_name[48];
    kstrncpy_proc(thread_name, name ? name : "thread", 40);

    int tid = create_thread(thread_name, (void (*)(void))entry_point, 5);
    if (tid >= 0 && proc->thread_count < MAX_THREADS_PER_PROC) {
        proc->thread_ids[proc->thread_count++] = (u32)tid;
    }

    proc_count++;
    return (int)proc->pid;
}

/*
 * Função: terminate_process
 * Descrição: Finaliza o processo com o PID especificado, liberando todos
 *            os recursos alocados incluindo memória, threads e arquivos abertos.
 */
void terminate_process(u32 pid, s32 exit_code) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *proc = &proc_table[i];
        if (proc->pid != pid || proc->state == PROC_EMPTY) continue;

        /* Termina todas as threads do processo */
        for (u32 t = 0; t < proc->thread_count; t++) {
            terminate_thread(proc->thread_ids[t]);
        }

        /* Libera a pilha de user mode */
        if (proc->user_stack) {
            kfree((void *)(proc->user_stack - USER_STACK_SIZE));
        }

        proc->exit_code = exit_code;
        proc->state     = PROC_ZOMBIE;

        /* Marca como vazio após coleta */
        proc->state = PROC_EMPTY;
        if (proc_count > 0) proc_count--;
        return;
    }
}

/*
 * Função: get_current_process
 * Descrição: Retorna o ponteiro para o processo atualmente em execução.
 */
process_t *get_current_process(void) {
    return current_proc;
}

/*
 * Função: get_process_by_pid
 * Descrição: Busca e retorna o processo com o PID especificado.
 *            Retorna NULL se não encontrado.
 */
process_t *get_process_by_pid(u32 pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_EMPTY) {
            return &proc_table[i];
        }
    }
    return (process_t *)NULL;
}

/*
 * Função: switch_to_user_mode
 * Descrição: Realiza a transição do modo kernel (ring 0) para o modo
 *            usuário (ring 3), configurando os segmentos e a pilha.
 *            Usa instrução IRET para a transição segura.
 *
 * Parâmetros:
 *   user_esp - Endereço do topo da pilha em modo usuário
 *   user_eip - Endereço de início da execução em modo usuário
 */
void switch_to_user_mode(u32 user_esp, u32 user_eip) {
    /*
     * Para a transição para user mode (ring 3), usamos IRET.
     * O IRET espera na pilha (em ordem de popping):
     *   EIP, CS, EFLAGS, ESP, SS
     *
     * Segmentos User Mode:
     *   CS = 0x1B (user code segment, RPL=3)
     *   SS = 0x23 (user data segment, RPL=3)
     *
     * Nota: Em um kernel completo, a GDT deve ter esses descriptores
     *       configurados. Aqui assumimos GDT com estrutura padrão.
     */
    __asm__ volatile (
        "cli\n"                         /* Desabilita interrupções */
        "mov $0x23, %%ax\n"             /* User data segment (ring 3) */
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x23\n"                  /* SS */
        "push %0\n"                     /* ESP de user mode */
        "pushfl\n"                      /* EFLAGS */
        "orl $0x200, (%%esp)\n"         /* Habilita IF no EFLAGS */
        "push $0x1B\n"                  /* CS de user mode (ring 3) */
        "push %1\n"                     /* EIP de user mode */
        "iret\n"                        /* Retorna para user mode */
        :
        : "r"(user_esp), "r"(user_eip)
        : "eax", "memory"
    );
}


//PARA x64
// void switch_to_user_mode(u64 user_rsp, u64 user_rip) {
//     __asm__ volatile (
//         "cli\n"
//         "mov $0x23, %%ax\n"
//         "mov %%ax, %%ds\n"
//         "mov %%ax, %%es\n"
//         "push $0x23\n"         // SS
//         "push %0\n"            // RSP
//         "pushfq\n"             // RFLAGS (64 bits)
//         "orq $0x200, (%%rsp)\n"// Habilita IF
//         "push $0x1B\n"         // CS (user mode ring 3)
//         "push %1\n"            // RIP
//         "iretq\n"              // iretq em vez de iret!
//         :
//         : "r"(user_rsp), "r"(user_rip)
//         : "rax", "memory"
//     );
// }

/*
 * Função: list_processes
 * Descrição: Exibe no terminal a lista de todos os processos ativos
 *            com seus PIDs, nomes, estados e uso de recursos.
 */
void list_processes(void) {
    const char *state_names[] = {
        "VAZIO", "EXECUTANDO", "ESPERANDO", "ZUMBI"
    };
    const char *priv_names[] = { "KERNEL", "USER" };

    screen_println("=== Lista de Processos ===");
    screen_println("  PID  Nome                  Estado       Priv    Threads");
    screen_println("  ---- --------------------- ------------ ------- -------");

    u32 count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_EMPTY) continue;

        screen_print("  ");
        screen_print_int((s32)p->pid);
        screen_print("    ");
        screen_print(p->name);
        screen_print("    ");

        u32 st = (u32)p->state;
        if (st < 4) screen_print(state_names[st]);
        screen_print("    ");

        u32 pr = (u32)p->privilege;
        if (pr < 2) screen_print(priv_names[pr]);
        screen_print("    ");

        screen_print_int((s32)p->thread_count);
        screen_println("");
        count++;
    }

    screen_print("Total de processos: ");
    screen_print_int((s32)count);
    screen_println("");
}

/*
 * Função: exec_program
 * Descrição: Carrega e executa um programa ELF a partir do caminho
 *            especificado, criando um novo processo para ele.
 */
int exec_program(const char *path) {
    if (!path) return -1;

    elf_context_t ctx;
    int result = load_elf_from_file(path, &ctx);
    if (result != 0) {
        screen_print("Erro ao carregar ELF: ");
        screen_print(path);
        screen_print(" (codigo: ");
        screen_print_int(result);
        screen_println(")");
        return result;
    }

    /* Cria o processo para o programa carregado */
    int pid = create_process(path, ctx.entry_point, PRIVILEGE_USER);
    if (pid < 0) {
        screen_println("Erro: nao foi possivel criar processo.");
        return -1;
    }

    screen_print("Programa carregado. PID: ");
    screen_print_int(pid);
    screen_print(", Entry: ");
    screen_print_hex(ctx.entry_point);
    screen_println("");

    return pid;
}
