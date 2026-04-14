/*
 * pci.c - Driver de barramento PCI
 * Descrição: Implementação real de detecção e enumeração de dispositivos
 *            PCI usando leitura do barramento via portas de I/O do sistema.
 *
 * O barramento PCI é acessado via dois registradores de 32 bits:
 *   - CONFIG_ADDRESS (0xCF8): define o endereço a ler
 *   - CONFIG_DATA    (0xCFC): retorna o dado do endereço configurado
 *
 * Formato do endereço CONFIG_ADDRESS:
 *   Bit 31    : Enable (deve ser 1)
 *   Bits 23-16: Número do barramento (0-255)
 *   Bits 15-11: Número do dispositivo (0-31)
 *   Bits 10-8 : Número da função (0-7)
 *   Bits 7-2  : Offset do registrador (múltiplo de 4)
 *   Bits 1-0  : Zero
 */

#include "../include/kernel.h"
#include "../include/devices.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(u32 val);
extern int register_device(device_type_t type, const char *name,
                           const char *info, u32 base_addr, u32 irq);

/* ============================================================
 * CONSTANTES DO BARRAMENTO PCI
 * ============================================================ */
#define PCI_CONFIG_ADDRESS  0xCF8   /* Porta de endereço de configuração PCI */
#define PCI_CONFIG_DATA     0xCFC   /* Porta de dados de configuração PCI */
#define PCI_ENABLE_BIT      0x80000000U

/* Offsets dos registradores PCI (Configuration Space Header) */
#define PCI_VENDOR_ID       0x00    /* ID do fabricante (16 bits) */
#define PCI_DEVICE_ID       0x02    /* ID do dispositivo (16 bits) */
#define PCI_COMMAND         0x04    /* Registrador de comando */
#define PCI_STATUS          0x06    /* Registrador de status */
#define PCI_REVISION_ID     0x08    /* ID de revisão (8 bits) */
#define PCI_PROG_IF         0x09    /* Programming Interface (8 bits) */
#define PCI_SUBCLASS        0x0A    /* Subclasse do dispositivo */
#define PCI_CLASS_CODE      0x0B    /* Código de classe */
#define PCI_HEADER_TYPE     0x0E    /* Tipo do cabeçalho */
#define PCI_BAR0            0x10    /* Base Address Register 0 */
#define PCI_INTERRUPT_LINE  0x3C    /* Linha de interrupção (IRQ) */

/* Valor inválido: se VENDOR_ID == 0xFFFF, o slot está vazio */
#define PCI_INVALID_VENDOR  0xFFFF

/* Limites do barramento */
#define PCI_MAX_BUSES       256
#define PCI_MAX_DEVICES     32
#define PCI_MAX_FUNCTIONS   8
#define PCI_MAX_STORED      64  /* Máximo de dispositivos armazenados */

/* ============================================================
 * ESTRUTURA DE DISPOSITIVO PCI
 * ============================================================ */
typedef struct {
    u8  bus;            /* Número do barramento */
    u8  device;         /* Número do dispositivo */
    u8  function;       /* Número da função */
    u16 vendor_id;      /* ID do fabricante */
    u16 device_id;      /* ID do dispositivo */
    u8  class_code;     /* Classe principal */
    u8  subclass;       /* Subclasse */
    u8  prog_if;        /* Interface de programação */
    u8  revision;       /* Revisão */
    u8  irq;            /* Linha de interrupção */
    u32 bar0;           /* Base Address Register 0 */
    char vendor_name[32];   /* Nome descritivo do fabricante */
    char class_name[32];    /* Nome descritivo da classe */
} pci_device_t;

/* ============================================================
 * VARIÁVEIS INTERNAS
 * ============================================================ */
static pci_device_t pci_devices[PCI_MAX_STORED];
static u32          pci_device_count = 0;

/* ============================================================
 * TABELA DE CLASSES PCI
 * ============================================================ */
typedef struct {
    u8          class_code;
    u8          subclass;
    const char *name;
} pci_class_entry_t;

static const pci_class_entry_t pci_class_table[] = {
    {0x00, 0x00, "Dispositivo Nao Classificado"},
    {0x01, 0x00, "Controlador SCSI"},
    {0x01, 0x01, "Controlador IDE"},
    {0x01, 0x06, "Controlador SATA (AHCI)"},
    {0x02, 0x00, "Controlador Ethernet"},
    {0x02, 0x80, "Controlador de Rede"},
    {0x03, 0x00, "Controlador VGA"},
    {0x03, 0x01, "Controlador XGA"},
    {0x04, 0x01, "Controlador de Audio"},
    {0x06, 0x00, "Host Bridge (CPU <-> PCI)"},
    {0x06, 0x01, "ISA Bridge"},
    {0x06, 0x04, "PCI-to-PCI Bridge"},
    {0x07, 0x00, "Controlador Serial"},
    {0x07, 0x01, "Controlador Paralelo"},
    {0x0C, 0x03, "Controlador USB"},
    {0x0C, 0x05, "Controlador SMBus"},
    {0xFF, 0xFF, "Dispositivo Desconhecido"},
};

/* ============================================================
 * FUNÇÕES INTERNAS
 * ============================================================ */

/*
 * Função: pci_config_read32
 * Descrição: Lê um valor de 32 bits do espaço de configuração PCI
 *            no barramento, dispositivo, função e offset especificados.
 */
static u32 pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset) {
    u32 address = PCI_ENABLE_BIT                |
                  ((u32)bus    << 16)           |
                  ((u32)dev    << 11)           |
                  ((u32)func   << 8)            |
                  ((u32)(offset & 0xFC));

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/*
 * Função: pci_config_read16
 * Descrição: Lê um valor de 16 bits do espaço de configuração PCI.
 */
static u16 pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset) {
    u32 val = pci_config_read32(bus, dev, func, offset & ~3);
    return (u16)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

/*
 * Função: pci_config_read8
 * Descrição: Lê um valor de 8 bits do espaço de configuração PCI.
 */
static u8 pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset) {
    u32 val = pci_config_read32(bus, dev, func, offset & ~3);
    return (u8)((val >> ((offset & 3) * 8)) & 0xFF);
}

/*
 * Função: pci_get_class_name
 * Descrição: Retorna o nome descritivo da classe/subclasse PCI.
 */
static const char *pci_get_class_name(u8 class_code, u8 subclass) {
    const char *generic = "Dispositivo Desconhecido";
    const char *class_match = NULL;

    for (u32 i = 0; pci_class_table[i].class_code != 0xFF; i++) {
        const pci_class_entry_t *e = &pci_class_table[i];
        if (e->class_code == class_code && e->subclass == subclass) {
            return e->name;
        }
        if (e->class_code == class_code) {
            class_match = e->name;
        }
    }

    return class_match ? class_match : generic;
}

/*
 * Função: pci_detect_device
 * Descrição: Verifica se um dispositivo PCI existe no slot especificado
 *            e, se sim, lê suas informações e registra na tabela interna.
 */
void pci_detect_device(u8 bus, u8 dev, u8 func) {
    u16 vendor_id = pci_config_read16(bus, dev, func, PCI_VENDOR_ID);

    /* Slot vazio */
    if (vendor_id == PCI_INVALID_VENDOR || vendor_id == 0x0000) return;

    if (pci_device_count >= PCI_MAX_STORED) return;

    pci_device_t *d = &pci_devices[pci_device_count++];

    d->bus        = bus;
    d->device     = dev;
    d->function   = func;
    d->vendor_id  = vendor_id;
    d->device_id  = pci_config_read16(bus, dev, func, PCI_DEVICE_ID);
    d->class_code = pci_config_read8(bus, dev, func, PCI_CLASS_CODE);
    d->subclass   = pci_config_read8(bus, dev, func, PCI_SUBCLASS);
    d->prog_if    = pci_config_read8(bus, dev, func, PCI_PROG_IF);
    d->revision   = pci_config_read8(bus, dev, func, PCI_REVISION_ID);
    d->irq        = pci_config_read8(bus, dev, func, PCI_INTERRUPT_LINE);
    d->bar0       = pci_config_read32(bus, dev, func, PCI_BAR0);

    /* Classifica o tipo */
    const char *class_name = pci_get_class_name(d->class_code, d->subclass);
    for (u32 i = 0; i < 31 && class_name[i]; i++) {
        d->class_name[i] = class_name[i];
        d->class_name[i + 1] = '\0';
    }

    /* Registra no sistema de dispositivos */
    register_device(DEV_PCI, d->class_name, "PCI", d->bar0, d->irq);
}

/* ============================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_pci
 * Descrição: Inicializa o subsistema PCI, zerando a tabela de dispositivos.
 */
void init_pci(void) {
    pci_device_count = 0;
    for (u32 i = 0; i < PCI_MAX_STORED; i++) {
        pci_devices[i].vendor_id = PCI_INVALID_VENDOR;
    }
}

/*
 * Função: pci_scan_bus
 * Descrição: Realiza varredura completa do barramento PCI, verificando
 *            todos os slots e funções para detectar dispositivos presentes.
 *            Examina até 8 barramentos, 32 dispositivos e 8 funções cada.
 */
void pci_scan_bus(void) {
    pci_device_count = 0;

    for (u16 bus = 0; bus < 8; bus++) {
        for (u8 dev = 0; dev < PCI_MAX_DEVICES; dev++) {
            u16 vendor = pci_config_read16((u8)bus, dev, 0, PCI_VENDOR_ID);
            if (vendor == PCI_INVALID_VENDOR) continue;

            /* Verifica se é dispositivo multi-função */
            u8 header = pci_config_read8((u8)bus, dev, 0, PCI_HEADER_TYPE);

            if (header & 0x80) {
                /* Multi-função: verifica todas as 8 funções */
                for (u8 func = 0; func < PCI_MAX_FUNCTIONS; func++) {
                    pci_detect_device((u8)bus, dev, func);
                }
            } else {
                pci_detect_device((u8)bus, dev, 0);
            }
        }
    }
}

/*
 * Função: pci_list_devices
 * Descrição: Exibe no terminal a lista completa de dispositivos PCI
 *            detectados, com barramento, dispositivo, IDs e classe.
 */
void pci_list_devices(void) {
    screen_println("=== Dispositivos PCI Detectados ===");
    screen_println("  Bus:Dev:Fn  Vendor   Device   Classe");
    screen_println("  ----------  ------   ------   ------");

    if (pci_device_count == 0) {
        screen_println("  Nenhum dispositivo PCI encontrado.");
        return;
    }

    for (u32 i = 0; i < pci_device_count; i++) {
        pci_device_t *d = &pci_devices[i];

        screen_print("  ");
        screen_print_int((s32)d->bus);
        screen_print(":");
        screen_print_int((s32)d->device);
        screen_print(":");
        screen_print_int((s32)d->function);
        screen_print("  ");
        screen_print_hex(d->vendor_id);
        screen_print("  ");
        screen_print_hex(d->device_id);
        screen_print("  ");
        screen_println(d->class_name);
    }

    screen_print("Total: ");
    screen_print_int((s32)pci_device_count);
    screen_println(" dispositivos PCI");
}

/*
 * Função: pci_get_device_count
 * Descrição: Retorna o número de dispositivos PCI detectados.
 */
u32 pci_get_device_count(void) {
    return pci_device_count;
}
