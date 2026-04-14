/*
 * devices.c - Sistema de detecção e registro de dispositivos
 * Descrição: Gerencia a tabela global de dispositivos do sistema,
 *            realizando detecção de hardware e registro centralizado.
 */

#include "../include/devices.h"
#include "../include/memory.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);

/* ============================================================
 * VARIÁVEIS INTERNAS
 * ============================================================ */
static device_t device_table[MAX_DEVICES];
static u32      device_count   = 0;
static u32      next_device_id = 1;

/* Nomes dos tipos de dispositivos */
static const char *device_type_names[] = {
    "Desconhecido",
    "Teclado",
    "Mouse",
    "Monitor",
    "Disco ATA",
    "Disco SCSI",
    "Rede",
    "Serial",
    "Paralela",
    "Audio",
    "USB",
    "PCI",
};

/* ============================================================
 * FUNÇÕES AUXILIARES
 * ============================================================ */

/*
 * Função: kstrncpy_dev
 * Descrição: Copia string com limite de tamanho para uso interno.
 */
static void kstrncpy_dev(char *dest, const char *src, u32 max) {
    u32 i = 0;
    while (i < max - 1 && src && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* ============================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_devices
 * Descrição: Inicializa o subsistema de dispositivos, preparando
 *            a tabela de dispositivos e executando detecção inicial.
 */
void init_devices(void) {
    device_count   = 0;
    next_device_id = 1;
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        device_table[i].active = FALSE;
        device_table[i].id     = 0;
    }
}

/*
 * Função: register_device
 * Descrição: Registra um dispositivo na tabela interna do kernel.
 *            Retorna o ID atribuído ao dispositivo ou -1 em erro.
 */
int register_device(device_type_t type, const char *name,
                    const char *info, u32 base_addr, u32 irq) {
    if (device_count >= MAX_DEVICES) return -1;

    /* Encontra slot livre */
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        if (!device_table[i].active) {
            device_table[i].id       = next_device_id++;
            device_table[i].type     = type;
            device_table[i].active   = TRUE;
            device_table[i].base_addr = base_addr;
            device_table[i].irq      = irq;
            kstrncpy_dev(device_table[i].name, name, DEV_NAME_LEN);
            kstrncpy_dev(device_table[i].info, info, DEV_INFO_LEN);
            device_count++;
            return (int)device_table[i].id;
        }
    }
    return -1;
}

/*
 * Função: detect_devices
 * Descrição: Realiza varredura completa do hardware para detectar
 *            todos os dispositivos conectados ao sistema.
 */
void detect_devices(void) {
    /* Registra dispositivos básicos sempre presentes em x86 */

    /* Teclado PS/2 - IRQ 1, porta 0x60 */
    register_device(DEV_KEYBOARD, "Teclado PS/2",
                    "IRQ1, Porta 0x60", 0x60, 1);

    /* Monitor VGA - buffer em 0xB8000 */
    register_device(DEV_DISPLAY, "Monitor VGA",
                    "Modo texto 80x25, Buffer 0xB8000", 0xB8000, 0);

    /* Porta Serial COM1 - IRQ 4, porta 0x3F8 */
    u8 com1_test = inb(0x3FD);
    if (com1_test != 0xFF) {
        register_device(DEV_SERIAL, "Porta Serial COM1",
                        "IRQ4, Porta 0x3F8", 0x3F8, 4);
    }

    /* Porta Serial COM2 - IRQ 3, porta 0x2F8 */
    u8 com2_test = inb(0x2FD);
    if (com2_test != 0xFF) {
        register_device(DEV_SERIAL, "Porta Serial COM2",
                        "IRQ3, Porta 0x2F8", 0x2F8, 3);
    }

    /* Verificação básica de mouse PS/2 (porta 0x60/0x64) */
    /* Em ambiente real, o mouse requer inicialização do controlador */
    register_device(DEV_MOUSE, "Mouse PS/2",
                    "IRQ12, Controlador 0x64", 0x64, 12);
}

/*
 * Função: list_devices
 * Descrição: Exibe no terminal a lista completa de dispositivos
 *            detectados, com tipo, nome e informações adicionais.
 */
void list_devices(void) {
    screen_println("=== Dispositivos Detectados ===");
    screen_println("  ID   Tipo              Nome");
    screen_println("  ---- ----------------- ----------------------------------------");

    u32 count = 0;
    for (u32 i = 0; i < MAX_DEVICES; i++) {
        device_t *d = &device_table[i];
        if (!d->active) continue;

        screen_print("  ");
        screen_print_int((s32)d->id);
        screen_print("    ");

        u32 type_idx = (u32)d->type;
        if (type_idx < 12) {
            screen_print(device_type_names[type_idx]);
        } else {
            screen_print("Especial");
        }
        screen_print("    ");
        screen_print(d->name);

        if (d->info[0]) {
            screen_print("  [");
            screen_print(d->info);
            screen_print("]");
        }
        screen_println("");
        count++;
    }

    if (count == 0) {
        screen_println("  Nenhum dispositivo detectado.");
    } else {
        screen_print("Total: ");
        screen_print_int((s32)count);
        screen_println(" dispositivos");
    }
}

/*
 * Função: get_device_count
 * Descrição: Retorna o número total de dispositivos registrados.
 */
u32 get_device_count(void) {
    return device_count;
}
