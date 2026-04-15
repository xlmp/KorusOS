/*
 * shell.c - Shell interativo do Korus OS
 * Descrição: Shell em modo texto com comandos inspirados no Linux.
 *            Interface principal de interação do usuário com o sistema.
 *
 * Comandos disponíveis:
 *   help, mem, devices, disk, net, threads, ps, ls, run, clear, echo, pci
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/disk.h"
#include "../include/devices.h"
#include "../include/vfs.h"

/* Declarações de funções externas */
extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_set_color(u8 fg, u8 bg);
extern void screen_clear(void);
extern void kb_readline(char *buf, u32 max);
extern void show_memory_usage(void);
extern void list_devices(void);
extern void show_disk_info(void);
extern void show_network_status(void);
extern void list_threads(void);
extern void list_processes(void);
extern void vfs_list(const char *path);
extern void pci_list_devices(void);
extern int  exec_program(const char *path);

/* ============================================================
 * CONSTANTES DO SHELL
 * ============================================================ */
#define SHELL_INPUT_SIZE    256     /* Tamanho máximo de linha de entrada */
#define SHELL_MAX_ARGS      16      /* Máximo de argumentos por comando */
#define SHELL_PROMPT        "korus> "

/* ============================================================
 * FUNÇÕES AUXILIARES DO SHELL
 * ============================================================ */

/*
 * Função: kstrlen
 * Descrição: Calcula o comprimento de uma string.
 */
static u32 kstrlen(const char *s) {
    u32 len = 0;
    while (s && s[len]) len++;
    return len;
}

/*
 * Função: kstrcmp
 * Descrição: Compara duas strings. Retorna 0 se iguais.
 */
static int kstrcmp(const char *a, const char *b) {
    if (!a || !b) return -1;
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/*
 * Função: kstrncmp
 * Descrição: Compara até n caracteres de duas strings.
 */
static int kstrncmp(const char *a, const char *b, u32 n) {
    while (n && *a && *b && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/*
 * Função: parse_args
 * Descrição: Divide a linha de entrada em argumentos separados por espaços.
 *            Modifica o buffer de entrada inserindo '\0' como separador.
 *            Retorna o número de argumentos encontrados.
 */
static int parse_args(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *p  = line;

    while (*p && argc < max_args) {
        /* Pula espaços iniciais */
        while (*p == ' ') p++;
        if (!*p) break;

        /* Início de um argumento */
        argv[argc++] = p;

        /* Avança até o próximo espaço ou fim da string */
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }

    return argc;
}

/* ============================================================
 * HANDLERS DE COMANDOS
 * ============================================================ */

/*
 * Função: cmd_help
 * Descrição: Exibe a lista de comandos disponíveis no shell com descrição.
 */
static void cmd_help(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);

    screen_set_color(0x0E, 0x00); /* Amarelo */
    screen_println("=== Korus OS Shell - Comandos Disponíveis ===");
    screen_set_color(0x07, 0x00);
    screen_println("");

    screen_set_color(0x0B, 0x00); /* Ciano */
    screen_print("  help     "); screen_set_color(0x07, 0x00);
    screen_println("- Exibe esta lista de comandos");

    screen_set_color(0x0B, 0x00);
    screen_print("  mem      "); screen_set_color(0x07, 0x00);
    screen_println("- Exibe uso de memória RAM");

    screen_set_color(0x0B, 0x00);
    screen_print("  devices  "); screen_set_color(0x07, 0x00);
    screen_println("- Lista dispositivos detectados");

    screen_set_color(0x0B, 0x00);
    screen_print("  pci      "); screen_set_color(0x07, 0x00);
    screen_println("- Lista dispositivos PCI do barramento");

    screen_set_color(0x0B, 0x00);
    screen_print("  disk     "); screen_set_color(0x07, 0x00);
    screen_println("- Informações sobre discos ATA");

    screen_set_color(0x0B, 0x00);
    screen_print("  net      "); screen_set_color(0x07, 0x00);
    screen_println("- Status da rede TCP/IP");

    screen_set_color(0x0B, 0x00);
    screen_print("  threads  "); screen_set_color(0x07, 0x00);
    screen_println("- Lista threads do sistema");

    screen_set_color(0x0B, 0x00);
    screen_print("  ps       "); screen_set_color(0x07, 0x00);
    screen_println("- Lista processos em execução");

    screen_set_color(0x0B, 0x00);
    screen_print("  ls [dir] "); screen_set_color(0x07, 0x00);
    screen_println("- Lista arquivos do diretório");

    screen_set_color(0x0B, 0x00);
    screen_print("  run <arq>"); screen_set_color(0x07, 0x00);
    screen_println("- Executa um programa ELF");

    screen_set_color(0x0B, 0x00);
    screen_print("  echo <tx>"); screen_set_color(0x07, 0x00);
    screen_println("- Exibe texto na tela");

    screen_set_color(0x0B, 0x00);
    screen_print("  clear    "); screen_set_color(0x07, 0x00);
    screen_println("- Limpa a tela do terminal");

    screen_set_color(0x0B, 0x00);
    screen_print("  version  "); screen_set_color(0x07, 0x00);
    screen_println("- Exibe versão do sistema");

    screen_println("");
}

/*
 * Função: cmd_mem
 * Descrição: Exibe informações de uso de memória RAM do sistema.
 */
static void cmd_mem(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    show_memory_usage();
}

/*
 * Função: cmd_devices
 * Descrição: Lista todos os dispositivos detectados no sistema.
 */
static void cmd_devices(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    list_devices();
}

/*
 * Função: cmd_pci
 * Descrição: Lista todos os dispositivos PCI detectados no barramento.
 */
static void cmd_pci(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    pci_list_devices();
}

/*
 * Função: cmd_disk
 * Descrição: Exibe informações sobre os discos ATA detectados.
 */
static void cmd_disk(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    show_disk_info();
}

/*
 * Função: cmd_net
 * Descrição: Exibe o status atual da interface de rede TCP/IP.
 */
static void cmd_net(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    show_network_status();
}

/*
 * Função: cmd_threads
 * Descrição: Lista todas as threads ativas no sistema com seus estados.
 */
static void cmd_threads(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    list_threads();
}

/*
 * Função: cmd_ps
 * Descrição: Lista todos os processos em execução no sistema.
 */
static void cmd_ps(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    list_processes();
}

/*
 * Função: cmd_ls
 * Descrição: Lista os arquivos e diretórios no caminho especificado.
 *            Usa "/" como diretório padrão se não especificado.
 */
static void cmd_ls(int argc, char **argv) {
    const char *path = "/";
    if (argc >= 2 && argv[1]) {
        path = argv[1];
    }
    vfs_list(path);
}

/*
 * Função: cmd_run
 * Descrição: Carrega e executa um programa ELF no caminho especificado.
 */
static void cmd_run(int argc, char **argv) {
    if (argc < 2 || !argv[1]) {
        screen_println("Uso: run <caminho_do_programa>");
        screen_println("Exemplo: run /bin/hello.elf");
        return;
    }

    screen_print("Carregando: ");
    screen_println(argv[1]);

    /* Monta o caminho com '/' inicial se necessário */
    char path[256];
    if (argv[1][0] != '/') {
        path[0] = '/';
        u32 i = 0;
        while (argv[1][i] && i < 254) {
            path[i + 1] = argv[1][i];
            i++;
        }
        path[i + 1] = '\0';
    } else {
        u32 i = 0;
        while (argv[1][i] && i < 255) {
            path[i] = argv[1][i];
            i++;
        }
        path[i] = '\0';
    }

    int result = exec_program(path);
    if (result < 0) {
        screen_set_color(0x0C, 0x00);
        screen_println("Erro: nao foi possivel executar o programa.");
        screen_set_color(0x07, 0x00);
    }
}

/*
 * Função: cmd_echo
 * Descrição: Exibe o texto fornecido como argumento no terminal.
 */
static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i]) {
            screen_print(argv[i]);
            if (i < argc - 1) screen_print(" ");
        }
    }
    screen_println("");
}

/*
 * Função: cmd_clear
 * Descrição: Limpa a tela do terminal e posiciona o cursor no início.
 */
static void cmd_clear(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    screen_clear();
}

/*
 * Função: cmd_version
 * Descrição: Exibe a versão do sistema operacional e informações de build.
 */
static void cmd_version(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    screen_set_color(0x0A, 0x00);
    screen_println("Korus OS - Experimental Operating System");
    screen_set_color(0x07, 0x00);
    screen_println("Versao  : 0.1.0");
    screen_println("Kernel  : Korus OS Kernel x86");
    screen_println("Compiler: LLVM Clang");
    screen_println("Arch    : x86 (i686)");
    screen_println("Boot    : GRUB Multiboot");
    screen_println("");
}

/* ============================================================
 * TABELA DE COMANDOS
 * ============================================================ */
typedef struct {
    const char *name;
    void (*handler)(int argc, char **argv);
    const char *description;
} shell_command_t;

static const shell_command_t commands[] = {
    { "help",    cmd_help,    "Exibe ajuda" },
    { "mem",     cmd_mem,     "Uso de memoria" },
    { "devices", cmd_devices, "Lista dispositivos" },
    { "pci",     cmd_pci,     "Dispositivos PCI" },
    { "disk",    cmd_disk,    "Informacoes de disco" },
    { "net",     cmd_net,     "Status da rede" },
    { "threads", cmd_threads, "Lista threads" },
    { "ps",      cmd_ps,      "Lista processos" },
    { "ls",      cmd_ls,      "Lista arquivos" },
    { "run",     cmd_run,     "Executa programa" },
    { "echo",    cmd_echo,    "Exibe texto" },
    { "clear",   cmd_clear,   "Limpa a tela" },
    { "version", cmd_version, "Versao do sistema" },
    { (const char *)NULL,      (void (*)(int, char **))NULL,        (const char *)NULL },
};

/* ============================================================
 * LOOP PRINCIPAL DO SHELL
 * ============================================================ */

/*
 * Função: shell_print_prompt
 * Descrição: Exibe o prompt do shell com a cor configurada.
 */
static void shell_print_prompt(void) {
    screen_set_color(0x0A, 0x00); /* Verde claro */
    screen_print(SHELL_PROMPT);
    screen_set_color(0x07, 0x00); /* Cinza padrão */
}

/*
 * Função: shell_execute
 * Descrição: Processa e executa uma linha de comando digitada pelo usuário.
 *            Analisa o comando, busca na tabela e chama o handler correspondente.
 */
static void shell_execute(char *line) {
    if (!line || !line[0]) return;

    /* Remove espaços no início */
    while (*line == ' ') line++;
    if (!*line) return;

    /* Faz parse dos argumentos */
    char *argv[SHELL_MAX_ARGS];
    int   argc = parse_args(line, argv, SHELL_MAX_ARGS);
    if (argc == 0) return;

    /* Busca o comando na tabela */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (kstrcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    /* Comando não encontrado */
    screen_set_color(0x0C, 0x00); /* Vermelho */
    screen_print("Comando nao encontrado: '");
    screen_print(argv[0]);
    screen_println("'");
    screen_set_color(0x07, 0x00);
    screen_println("Digite 'help' para ver os comandos disponíveis.");

    UNUSED(kstrlen);
    UNUSED(kstrncmp);
}

/*
 * Função: shell_run
 * Descrição: Função principal do shell. Executa o loop de leitura e
 *            processamento de comandos até o sistema ser desligado.
 *            Este loop nunca retorna em operação normal.
 */
void shell_run(void) {
    char input_buffer[SHELL_INPUT_SIZE];

    while (TRUE) {
        /* Exibe o prompt */
        shell_print_prompt();

        /* Lê a linha do teclado */
        kb_readline(input_buffer, SHELL_INPUT_SIZE);

        /* Processa o comando */
        shell_execute(input_buffer);
    }
}
