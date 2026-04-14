#ifndef DISK_H
#define DISK_H

/*
 * disk.h - Cabeçalho do driver de disco
 * Descrição: Define estruturas e protótipos para acesso a disco via
 *            protocolo ATA/IDE (PIO mode) para leitura e escrita de setores.
 */

#include "kernel.h"

/* ============================================================
 * CONSTANTES ATA/IDE
 * ============================================================ */
#define ATA_PRIMARY_BASE    0x1F0   /* Porta base do canal ATA primário */
#define ATA_SECONDARY_BASE  0x170   /* Porta base do canal ATA secundário */
#define ATA_PRIMARY_CTRL    0x3F6   /* Porta de controle ATA primário */
#define ATA_SECONDARY_CTRL  0x376   /* Porta de controle ATA secundário */

/* Offsets de registradores ATA */
#define ATA_REG_DATA        0x00    /* Registrador de dados */
#define ATA_REG_ERROR       0x01    /* Registrador de erro */
#define ATA_REG_FEATURES    0x01    /* Registrador de features */
#define ATA_REG_SECCOUNT    0x02    /* Contador de setores */
#define ATA_REG_LBA_LOW     0x03    /* LBA bits 0-7 */
#define ATA_REG_LBA_MID     0x04    /* LBA bits 8-15 */
#define ATA_REG_LBA_HIGH    0x05    /* LBA bits 16-23 */
#define ATA_REG_HDDEVSEL    0x06    /* Seletor de dispositivo/drive */
#define ATA_REG_STATUS      0x07    /* Registrador de status */
#define ATA_REG_COMMAND     0x07    /* Registrador de comando */

/* Bits de status ATA */
#define ATA_SR_BSY          0x80    /* Drive ocupado (busy) */
#define ATA_SR_DRDY         0x40    /* Drive pronto */
#define ATA_SR_DRQ          0x08    /* Requisição de dados */
#define ATA_SR_ERR          0x01    /* Erro ocorrido */

/* Comandos ATA */
#define ATA_CMD_READ_PIO    0x20    /* Leitura em modo PIO */
#define ATA_CMD_WRITE_PIO   0x30    /* Escrita em modo PIO */
#define ATA_CMD_IDENTIFY    0xEC    /* Identificar dispositivo */
#define ATA_CMD_FLUSH       0xE7    /* Flush do cache */

#define ATA_SECTOR_SIZE     512     /* Tamanho de um setor em bytes */
#define MAX_DRIVES          4       /* Máximo de drives suportados */

/* ============================================================
 * ESTRUTURAS
 * ============================================================ */

/* Informações de um drive ATA */
typedef struct {
    bool    present;            /* Drive está presente */
    bool    is_master;          /* É o dispositivo mestre */
    u16     base_port;          /* Porta base do canal */
    u16     ctrl_port;          /* Porta de controle */
    u32     size_sectors;       /* Tamanho total em setores */
    u64     size_bytes;         /* Tamanho total em bytes */
    char    model[41];          /* Modelo do drive */
    char    serial[21];         /* Número de série */
} drive_info_t;

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: init_disk
 * Descrição: Inicializa o subsistema de disco, detectando e identificando
 *            todos os drives ATA conectados ao sistema.
 */
void init_disk(void);

/*
 * Função: disk_read
 * Descrição: Lê um ou mais setores do disco a partir do endereço LBA
 *            especificado e armazena os dados no buffer fornecido.
 *            Retorna 0 em sucesso ou código de erro negativo.
 */
int disk_read(u8 drive, u32 lba, u8 sector_count, void *buffer);

/*
 * Função: disk_write
 * Descrição: Escreve um ou mais setores no disco a partir do endereço LBA
 *            especificado usando os dados do buffer fornecido.
 *            Retorna 0 em sucesso ou código de erro negativo.
 */
int disk_write(u8 drive, u32 lba, u8 sector_count, const void *buffer);

/*
 * Função: mount_filesystem
 * Descrição: Monta o sistema de arquivos do drive especificado,
 *            lendo o setor de boot e identificando o tipo de FS.
 */
int mount_filesystem(u8 drive);

/*
 * Função: list_files
 * Descrição: Lista os arquivos e diretórios no caminho especificado,
 *            exibindo informações como nome, tamanho e data.
 */
void list_files(const char *path);

/*
 * Função: get_drive_info
 * Descrição: Retorna ponteiro para as informações do drive especificado.
 *            Retorna NULL se o drive não existe ou não está presente.
 */
drive_info_t *get_drive_info(u8 drive);

/*
 * Função: show_disk_info
 * Descrição: Exibe no terminal informações sobre todos os drives detectados.
 */
void show_disk_info(void);

#endif /* DISK_H */
