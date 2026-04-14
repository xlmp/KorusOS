#ifndef DEVICES_H
#define DEVICES_H

/*
 * devices.h - Cabeçalho do sistema de detecção de dispositivos
 * Descrição: Define estruturas e protótipos para detecção, registro
 *            e listagem de dispositivos de hardware conectados ao sistema.
 */

#include "kernel.h"

/* ============================================================
 * TIPOS DE DISPOSITIVOS
 * ============================================================ */
typedef enum {
    DEV_UNKNOWN     = 0x00,
    DEV_KEYBOARD    = 0x01,
    DEV_MOUSE       = 0x02,
    DEV_DISPLAY     = 0x03,
    DEV_DISK_ATA    = 0x04,
    DEV_DISK_SCSI   = 0x05,
    DEV_NETWORK     = 0x06,
    DEV_SERIAL      = 0x07,
    DEV_PARALLEL    = 0x08,
    DEV_SOUND       = 0x09,
    DEV_USB         = 0x0A,
    DEV_PCI         = 0x0B,
} device_type_t;

/* ============================================================
 * ESTRUTURA DE DISPOSITIVO
 * ============================================================ */
#define MAX_DEVICES     64
#define DEV_NAME_LEN    64
#define DEV_INFO_LEN    128

typedef struct {
    u32             id;                 /* ID único do dispositivo */
    device_type_t   type;               /* Tipo do dispositivo */
    char            name[DEV_NAME_LEN]; /* Nome descritivo */
    char            info[DEV_INFO_LEN]; /* Informação adicional */
    bool            active;             /* Dispositivo ativo/presente */
    u32             base_addr;          /* Endereço de I/O base */
    u32             irq;                /* IRQ associado */
} device_t;

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: init_devices
 * Descrição: Inicializa o subsistema de dispositivos, preparando
 *            a tabela de dispositivos e executando detecção inicial.
 */
void init_devices(void);

/*
 * Função: detect_devices
 * Descrição: Realiza varredura completa do hardware para detectar
 *            todos os dispositivos conectados ao sistema, incluindo
 *            teclado, mouse, disco, rede e dispositivos PCI.
 */
void detect_devices(void);

/*
 * Função: register_device
 * Descrição: Registra um dispositivo na tabela interna do kernel.
 *            Retorna o ID atribuído ao dispositivo ou -1 em erro.
 */
int register_device(device_type_t type, const char *name,
                    const char *info, u32 base_addr, u32 irq);

/*
 * Função: list_devices
 * Descrição: Exibe no terminal a lista completa de dispositivos
 *            detectados, com tipo, nome e informações adicionais.
 */
void list_devices(void);

/*
 * Função: get_device_count
 * Descrição: Retorna o número total de dispositivos registrados.
 */
u32 get_device_count(void);

#endif /* DEVICES_H */
