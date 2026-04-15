/*
 * kernel.c - Kernel principal do OS experimental
 * Descrição: Ponto de entrada do kernel em C. Inicializa todos os
 *            subsistemas do sistema operacional na ordem correta e
 *            inicia o shell interativo para o usuário.
 *
 * Compilador: Clang (LLVM)
 * Arquitetura: x86 (32 bits)
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/disk.h"
#include "../include/devices.h"
#include "../include/vfs.h"
#include "../include/syscall.h"

/* Declarações externas dos drivers e módulos */
extern void init_screen(void);
extern void screen_clear(void);
extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_hex(u32 val);
extern void screen_print_int(s32 val);
extern void screen_set_color(u8 fg, u8 bg);
extern void init_keyboard(void);
extern void init_pci(void);
extern void pci_scan_bus(void);
extern void init_network(void);
extern void shell_run(void);

/* ============================================================
 * VERSÃO DO SISTEMA
 * ============================================================ */
#define OS_NAME         "Korus OS"
#define OS_VERSION      "0.1.0"
#define OS_ARCH         "x86"
#define MULTIBOOT_EXPECTED_MAGIC 0x2BADB002

/* ============================================================
 * FUNÇÕES UTILITÁRIAS INTERNAS
 * ============================================================ */

/*
 * Função: print_banner
 * Descrição: Exibe o banner de boas-vindas do sistema operacional
 *            com nome, versão e informações de copyright.
 */
static void print_banner(void) {
    screen_set_color(0x0A, 0x00); /* Verde claro no fundo preto */
    screen_println("╔--------------------------------------------------------------------------╗");
    screen_println("|                          Korus OS - Experimental OS v0.1.0                  |");
    screen_println("╚--------------------------------------------------------------------------╝");
    screen_set_color(0x07, 0x00); /* Cinza claro padrão */
    screen_println("");
}

/*
 * Função: print_init_step
 * Descrição: Exibe uma mensagem de inicialização de subsistema no formato
 *            padronizado "[OK] mensagem".
 */
static void print_init_step(const char *msg) {
    screen_set_color(0x0A, 0x00); /* Verde */
    screen_print("[OK] ");
    screen_set_color(0x07, 0x00); /* Cinza */
    screen_println(msg);
}

/*
 * Função: print_fail_step
 * Descrição: Exibe uma mensagem de falha de inicialização no formato
 *            "[ERRO] mensagem".
 */
static void print_fail_step(const char *msg) {
    screen_set_color(0x0C, 0x00); /* Vermelho */
    screen_print("[ERRO] ");
    screen_set_color(0x07, 0x00);
    screen_println(msg);
}

/* ============================================================
 * PONTO DE ENTRADA DO KERNEL
 * ============================================================ */

/*
 * Função: kernel_main
 * Descrição: Função principal do kernel chamada pelo bootloader (boot.asm).
 *            Recebe o magic number do Multiboot e o ponteiro para as
 *            informações de boot, inicializa todos os subsistemas e
 *            inicia o shell interativo.
 *
 * Parâmetros:
 *   magic - Número mágico fornecido pelo GRUB (deve ser 0x2BADB002)
 *   mbi   - Ponteiro para estrutura com informações de boot do Multiboot
 */
int create_thread(const char *name, void (*entry)(void), u32 priority);
void ThreadTeste(){
    while(1){
        // screen_println("Thread de teste rodando...");
    //     for (volatile u32 i = 0; i < 100000000; i++); /* Delay simples */
    }
}
extern "C"
void kernel_main(u32 magic, multiboot_info_t *mbi) {

    /* Inicializa o driver de vídeo (deve ser o primeiro) */
    init_screen();
    screen_clear();
    print_banner();

    /* Verifica o magic number do Multiboot */
    if (magic != MULTIBOOT_EXPECTED_MAGIC) {
        print_fail_step("Magic number Multiboot invalido!");
        kernel_panic("Boot invalido - nao inicializado pelo GRUB");
    }

    screen_println("Inicializando subsistemas do kernel...");
    screen_println("");

    /* ---- Memória ---- */
    u32 mem_total_kb = 0;
    if (mbi->flags & (1 << 0)) {
        /* mem_upper está em KB acima de 1MB */
        mem_total_kb = mbi->mem_lower + mbi->mem_upper;
    } else {
        mem_total_kb = 32 * 1024; /* Fallback: assume 32 MB */
    }

    init_memory(mem_total_kb);
    print_init_step("Gerenciamento de memoria e paginacao inicializados");

    /* ---- Teclado ---- */
    init_keyboard();
    print_init_step("Driver de teclado PS/2 inicializado");

    /* ---- Dispositivos e PCI ---- */
    init_devices();
    detect_devices();
    init_pci();
    pci_scan_bus();
    print_init_step("Deteccao de dispositivos e barramento PCI concluida");

    /* ---- Disco ---- */
    init_disk();
    print_init_step("Driver de disco ATA/IDE inicializado");

    /* ---- VFS ---- */
    vfs_init();
    print_init_step("Sistema de Arquivos Virtual (VFS) inicializado");

    /* ---- Scheduler e Processos ---- */
    init_scheduler();
    init_processes();
    print_init_step("Scheduler de threads e gerenciamento de processos OK");

    /* ---- Syscalls ---- */
    init_syscalls();
    print_init_step("Tabela de chamadas de sistema (syscalls) registrada");

    /* ---- Rede ---- */
    init_network();
    print_init_step("Subsistema de rede TCP/IP inicializado");

    /* Exibe info de memória detectada */
    screen_println("");
    screen_set_color(0x0B, 0x00); /* Ciano claro */
    screen_print("Memoria detectada: ");
    screen_print_int((s32)(mem_total_kb / 1024));
    screen_println(" MB");
    screen_set_color(0x07, 0x00);

    screen_println("");
    screen_set_color(0x0E, 0x00); /* Amarelo */
    screen_println("Sistema inicializado com sucesso! Digite 'help' para ajuda.");
    screen_set_color(0x07, 0x00);
    screen_println("");

    // create_thread("ThreadTeste", (void (*)(void))ThreadTeste, PRIVILEGE_USER);

    /* Inicia o shell interativo */
    shell_run();

    /* O shell nunca deve retornar; se retornar, entra em panico */
    kernel_panic("Shell encerrado inesperadamente!");
}

/* ============================================================
 * FUNÇÕES DE UTILITÁRIO DO KERNEL
 * ============================================================ */

/*
 * Função: kernel_panic
 * Descrição: Exibe uma mensagem de erro crítico (kernel panic) e
 *            interrompe completamente o sistema, desabilitando
 *            as interrupções e entrando em loop infinito.
 */
void kernel_panic(const char *msg) {
    /* Desabilita interrupções imediatamente */
    __asm__ volatile ("cli");

    screen_set_color(0x0C, 0x00); /* Vermelho */
    screen_println("");
    screen_println("*** KERNEL PANIC ***");
    screen_print("Erro: ");
    screen_println(msg);
    screen_println("Sistema travado. Reinicie o computador.");

    /* Loop infinito - sistema parado */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/*
 * Função: kernel_log
 * Descrição: Registra uma mensagem informativa do kernel no terminal.
 *            Usado para depuração e informações de sistema.
 */
void kernel_log(const char *msg) {
    screen_set_color(0x08, 0x00); /* Cinza escuro */
    screen_print("[LOG] ");
    screen_set_color(0x07, 0x00);
    screen_println(msg);
}
