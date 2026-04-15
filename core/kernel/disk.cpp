/*
 * disk.c - Driver de disco ATA/IDE em modo PIO
 * Descrição: Implementação de leitura e escrita de disco via protocolo
 *            ATA/IDE usando o modo PIO (Programmed I/O) sem DMA.
 *            Suporta até 4 drives (primário mestre/escravo, secundário M/S).
 */

#include "../include/disk.h"
#include "../include/memory.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(uptr val);

/* ============================================================
 * VARIÁVEIS INTERNAS
 * ============================================================ */
static drive_info_t drives[MAX_DRIVES];

/* Configuração dos 4 drives ATA */
static const struct {
    u16  base;
    u16  ctrl;
    bool master;
} drive_config[MAX_DRIVES] = {
    {ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   TRUE},   /* Drive 0: Primário Mestre */
    {ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   FALSE},  /* Drive 1: Primário Escravo */
    {ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, TRUE},   /* Drive 2: Secundário Mestre */
    {ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, FALSE},  /* Drive 3: Secundário Escravo */
};

/* ============================================================
 * FUNÇÕES AUXILIARES INTERNAS
 * ============================================================ */

/*
 * Função: ata_wait_bsy
 * Descrição: Aguarda o bit BSY (busy) do drive ser limpo.
 *            Retorna FALSE se ocorrer timeout.
 */
static bool ata_wait_bsy(u16 base) {
    u32 timeout = 100000;
    while ((inb(base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout--)
        io_wait();
    return timeout > 0;
}

/*
 * Função: ata_wait_drq
 * Descrição: Aguarda o bit DRQ (data request) ser ativado.
 *            Retorna FALSE se ocorrer timeout ou erro.
 */
static bool ata_wait_drq(u16 base) {
    u32 timeout = 100000;
    u8  status;
    do {
        status = inb(base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return FALSE;
        timeout--;
    } while (!(status & ATA_SR_DRQ) && timeout);
    return (timeout > 0) && (status & ATA_SR_DRQ);
}

/*
 * Função: ata_select_drive
 * Descrição: Seleciona o drive (mestre ou escravo) no barramento ATA.
 */
static void ata_select_drive(u16 base, bool master, u32 lba_top) {
    u8 sel = master ? 0xE0 : 0xF0;
    sel |= (u8)((lba_top >> 24) & 0x0F); /* LBA bits 24-27 */
    outb(base + ATA_REG_HDDEVSEL, sel);
    /* Aguarda 400ns (4 leituras da porta de controle) */
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
}

/*
 * Função: ata_identify
 * Descrição: Executa o comando IDENTIFY para obter informações do drive.
 *            Preenche a estrutura drive_info_t com modelo, série e tamanho.
 *            Retorna TRUE se o drive foi identificado com sucesso.
 */
static bool ata_identify(u8 drive_num, drive_info_t *drive) {
    u16 base = drive->base_port;
    bool master = drive->is_master;

    /* Seleciona o drive */
    outb(base + ATA_REG_HDDEVSEL, master ? 0xA0 : 0xB0);
    io_wait();

    /* Zera os registradores LBA */
    outb(base + ATA_REG_SECCOUNT, 0);
    outb(base + ATA_REG_LBA_LOW,  0);
    outb(base + ATA_REG_LBA_MID,  0);
    outb(base + ATA_REG_LBA_HIGH, 0);

    /* Envia comando IDENTIFY */
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    /* Verifica se o drive existe */
    u8 status = inb(base + ATA_REG_STATUS);
    if (status == 0x00) return FALSE;

    /* Aguarda o drive ficar pronto */
    if (!ata_wait_bsy(base)) return FALSE;

    /* Verifica se é ATAPI (não suportado aqui) */
    u8 mid  = inb(base + ATA_REG_LBA_MID);
    u8 high = inb(base + ATA_REG_LBA_HIGH);
    if (mid != 0 || high != 0) return FALSE; /* ATAPI - ignora */

    /* Aguarda DRQ */
    if (!ata_wait_drq(base)) return FALSE;

    /* Lê os 256 words de identificação */
    u16 identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(base + ATA_REG_DATA);
    }

    /* Extrai número de setores (LBA28) */
    drive->size_sectors = ((u32)identify_data[61] << 16) | identify_data[60];
    drive->size_bytes   = (u64)drive->size_sectors * ATA_SECTOR_SIZE;

    /* Extrai string de modelo (words 27-46, bytes invertidos) */
    u32 j = 0;
    for (int i = 27; i <= 46 && j < 40; i++) {
        drive->model[j++] = (char)(identify_data[i] >> 8);
        drive->model[j++] = (char)(identify_data[i] & 0xFF);
    }
    drive->model[40] = '\0';
    /* Remove espaços no final do modelo */
    for (int i = 39; i >= 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0';
    }

    /* Extrai número de série (words 10-19) */
    j = 0;
    for (int i = 10; i <= 19 && j < 20; i++) {
        drive->serial[j++] = (char)(identify_data[i] >> 8);
        drive->serial[j++] = (char)(identify_data[i] & 0xFF);
    }
    drive->serial[20] = '\0';

    drive->present = TRUE;
    return TRUE;
}

/* ============================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_disk
 * Descrição: Inicializa o subsistema de disco, detectando e identificando
 *            todos os drives ATA conectados ao sistema.
 */
void init_disk(void) {
    for (u8 i = 0; i < MAX_DRIVES; i++) {
        drives[i].present    = FALSE;
        drives[i].is_master  = drive_config[i].master;
        drives[i].base_port  = drive_config[i].base;
        drives[i].ctrl_port  = drive_config[i].ctrl;
        drives[i].size_sectors = 0;

        /* Tenta identificar o drive */
        ata_identify(i, &drives[i]);
    }
}

/*
 * Função: disk_read
 * Descrição: Lê um ou mais setores do disco a partir do endereço LBA
 *            especificado e armazena os dados no buffer fornecido.
 *            Retorna 0 em sucesso ou código de erro negativo.
 */
int disk_read(u8 drive_num, u32 lba, u8 sector_count, void *buffer) {
    if (drive_num >= MAX_DRIVES || !drives[drive_num].present) return -1;
    if (!buffer || sector_count == 0) return -2;

    drive_info_t *drv  = &drives[drive_num];
    u16           base = drv->base_port;
    bool          master = drv->is_master;

    if (!ata_wait_bsy(base)) return -3;

    ata_select_drive(base, master, lba);

    outb(base + ATA_REG_SECCOUNT, sector_count);
    outb(base + ATA_REG_LBA_LOW,  (u8)(lba & 0xFF));
    outb(base + ATA_REG_LBA_MID,  (u8)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    outb(base + ATA_REG_COMMAND,  ATA_CMD_READ_PIO);

    u16 *buf16 = (u16 *)buffer;
    for (u8 s = 0; s < sector_count; s++) {
        if (!ata_wait_bsy(base))  return -4;
        if (!ata_wait_drq(base))  return -5;

        for (int i = 0; i < 256; i++) {
            buf16[s * 256 + i] = inw(base + ATA_REG_DATA);
        }
    }

    return 0;
}

/*
 * Função: disk_write
 * Descrição: Escreve um ou mais setores no disco a partir do endereço LBA
 *            especificado usando os dados do buffer fornecido.
 *            Retorna 0 em sucesso ou código de erro negativo.
 */
int disk_write(u8 drive_num, u32 lba, u8 sector_count, const void *buffer) {
    if (drive_num >= MAX_DRIVES || !drives[drive_num].present) return -1;
    if (!buffer || sector_count == 0) return -2;

    drive_info_t *drv  = &drives[drive_num];
    u16           base = drv->base_port;
    bool          master = drv->is_master;

    if (!ata_wait_bsy(base)) return -3;

    ata_select_drive(base, master, lba);

    outb(base + ATA_REG_SECCOUNT, sector_count);
    outb(base + ATA_REG_LBA_LOW,  (u8)(lba & 0xFF));
    outb(base + ATA_REG_LBA_MID,  (u8)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    outb(base + ATA_REG_COMMAND,  ATA_CMD_WRITE_PIO);

    const u16 *buf16 = (const u16 *)buffer;
    for (u8 s = 0; s < sector_count; s++) {
        if (!ata_wait_bsy(base)) return -4;
        if (!ata_wait_drq(base)) return -5;

        for (int i = 0; i < 256; i++) {
            outw(base + ATA_REG_DATA, buf16[s * 256 + i]);
        }

        /* Flush do cache após cada setor */
        outb(base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
        ata_wait_bsy(base);
    }

    return 0;
}

/*
 * Função: get_drive_info
 * Descrição: Retorna ponteiro para as informações do drive especificado.
 */
drive_info_t *get_drive_info(u8 drive) {
    if (drive >= MAX_DRIVES) return (drive_info_t *)NULL;
    return &drives[drive];
}

/*
 * Função: show_disk_info
 * Descrição: Exibe no terminal informações sobre todos os drives detectados.
 */
void show_disk_info(void) {
    screen_println("=== Informacoes de Disco ===");
    bool found = FALSE;

    for (u8 i = 0; i < MAX_DRIVES; i++) {
        if (!drives[i].present) continue;
        found = TRUE;

        screen_print("  Drive ");
        screen_print_int((s32)i);
        screen_print(drives[i].is_master ? " (Mestre): " : " (Escravo): ");
        screen_println(drives[i].model);
        screen_print("    Tamanho: ");
        screen_print_int((s32)(drives[i].size_sectors / 2048));
        screen_println(" MB");
        screen_print("    Serie  : ");
        screen_println(drives[i].serial);
    }

    if (!found) {
        screen_println("  Nenhum drive ATA detectado.");
    }
}

/*
 * Função: mount_filesystem
 * Descrição: Monta o sistema de arquivos do drive especificado,
 *            lendo o setor de boot e identificando o tipo de FS.
 */
int mount_filesystem(u8 drive) {
    if (drive >= MAX_DRIVES || !drives[drive].present) return -1;

    u8 sector[ATA_SECTOR_SIZE];
    int result = disk_read(drive, 0, 1, sector);
    if (result != 0) return -2;

    /* Verifica assinatura de boot (0x55AA) */
    if (sector[510] == 0x55 && sector[511] == 0xAA) {
        return 0; /* Disco inicializável detectado */
    }

    return -3; /* Sem sistema de arquivos reconhecido */
}

/*
 * Função: list_files
 * Descrição: Lista os arquivos no caminho especificado (stub para integração VFS).
 */
void list_files(const char *path) {
    extern void vfs_list(const char *path);
    vfs_list(path);
}
