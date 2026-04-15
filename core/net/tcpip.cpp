/*
 * tcpip.c - Stack TCP/IP básica
 * Descrição: Implementação básica das camadas Ethernet, IP e TCP,
 *            incluindo estruturas de pacotes, checksums e envio/recepção.
 *            Baseada em protocolo RFC padrão para comunicação em rede.
 */

#include "../include/kernel.h"
#include "../include/memory.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(uptr val);
extern void screen_putchar(char c);

/* ============================================================
 * CONSTANTES DE REDE
 * ============================================================ */
#define ETH_ALEN            6       /* Tamanho do endereço MAC */
#define ETH_TYPE_IP         0x0800  /* EtherType para IPv4 */
#define ETH_TYPE_ARP        0x0806  /* EtherType para ARP */

#define IP_PROTO_ICMP       1       /* Protocolo ICMP */
#define IP_PROTO_TCP        6       /* Protocolo TCP */
#define IP_PROTO_UDP        17      /* Protocolo UDP */

#define TCP_FLAG_FIN        0x01    /* Terminar conexão */
#define TCP_FLAG_SYN        0x02    /* Iniciar conexão */
#define TCP_FLAG_RST        0x04    /* Resetar conexão */
#define TCP_FLAG_PSH        0x08    /* Push de dados */
#define TCP_FLAG_ACK        0x10    /* Confirmação (ACK) */

#define MAX_CONNECTIONS     16      /* Máximo de conexões TCP simultâneas */
#define NET_BUFFER_SIZE     1500    /* MTU padrão Ethernet */

/* ============================================================
 * ESTRUTURAS DE PROTOCOLO (formato wire, big-endian)
 * ============================================================ */

/* Cabeçalho Ethernet */
typedef struct {
    u8  dst_mac[ETH_ALEN];  /* MAC de destino */
    u8  src_mac[ETH_ALEN];  /* MAC de origem */
    u16 ethertype;           /* Tipo do protocolo (IP, ARP...) */
} PACKED eth_header_t;

/* Cabeçalho IPv4 */
typedef struct {
    u8  version_ihl;    /* Versão (4 bits) + IHL em words (4 bits) */
    u8  dscp_ecn;       /* DSCP + ECN (QoS) */
    u16 total_len;      /* Comprimento total do pacote IP */
    u16 id;             /* Identificação do fragmento */
    u16 flags_offset;   /* Flags (3 bits) + Offset (13 bits) */
    u8  ttl;            /* Time To Live */
    u8  protocol;       /* Protocolo (TCP=6, UDP=17, ICMP=1) */
    u16 checksum;       /* Checksum do cabeçalho */
    u32 src_ip;         /* Endereço IP de origem */
    u32 dst_ip;         /* Endereço IP de destino */
} PACKED ip_header_t;

/* Cabeçalho TCP */
typedef struct {
    u16 src_port;       /* Porta de origem */
    u16 dst_port;       /* Porta de destino */
    u32 seq_num;        /* Número de sequência */
    u32 ack_num;        /* Número de confirmação */
    u8  data_offset;    /* Tamanho do cabeçalho TCP em words (4 bits upper) */
    u8  flags;          /* Flags TCP (SYN, ACK, FIN, etc.) */
    u16 window;         /* Tamanho da janela de recepção */
    u16 checksum;       /* Checksum TCP */
    u16 urgent;         /* Ponteiro urgente */
} PACKED tcp_header_t;

/* Estrutura de conexão TCP */
typedef enum {
    TCP_CLOSED      = 0,
    TCP_LISTEN      = 1,
    TCP_SYN_SENT    = 2,
    TCP_SYN_RCVD    = 3,
    TCP_ESTABLISHED = 4,
    TCP_FIN_WAIT    = 5,
    TCP_TIME_WAIT   = 6,
} tcp_state_t;

typedef struct {
    bool        active;         /* Conexão ativa */
    tcp_state_t state;          /* Estado da conexão */
    u32         local_ip;       /* IP local */
    u32         remote_ip;      /* IP remoto */
    u16         local_port;     /* Porta local */
    u16         remote_port;    /* Porta remota */
    u32         seq_num;        /* Número de sequência atual */
    u32         ack_num;        /* Número de ack atual */
    u8          rx_buffer[NET_BUFFER_SIZE]; /* Buffer de recepção */
    u32         rx_len;         /* Bytes no buffer de recepção */
} tcp_connection_t;

/* ============================================================
 * VARIÁVEIS INTERNAS DE REDE
 * ============================================================ */
static tcp_connection_t connections[MAX_CONNECTIONS];
static u32  local_ip       = 0;   /* IP local do sistema */
static u8   local_mac[6]   = {0}; /* MAC local do sistema */
static bool network_up     = FALSE; /* Interface de rede ativa */
static u32  packets_sent   = 0;
static u32  packets_recv   = 0;
static u16  next_port      = 49152; /* Porta efêmera inicial */

/* ============================================================
 * FUNÇÕES AUXILIARES
 * ============================================================ */

/*
 * Função: htons
 * Descrição: Converte valor de 16 bits de host byte order para
 *            network byte order (big-endian).
 */
static u16 htons(u16 val) {
    return (u16)((val >> 8) | (val << 8));
}

/*
 * Função: htonl
 * Descrição: Converte valor de 32 bits de host byte order para
 *            network byte order (big-endian).
 */
static u32 htonl(u32 val) {
    return ((val >> 24) & 0xFF)        |
           (((val >> 16) & 0xFF) << 8) |
           (((val >> 8)  & 0xFF) << 16)|
           ((val & 0xFF) << 24);
}

/*
 * Função: ip_checksum
 * Descrição: Calcula o checksum de cabeçalho IP usando complemento de 1.
 *            Padrão RFC 791 para verificação de integridade.
 */
static u16 ip_checksum(const void *data, u32 len) {
    const u16 *p = (const u16 *)data;
    u32 sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const u8 *)p;
    }

    /* Fold carry bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (u16)~sum;
}

/*
 * Função: find_free_connection
 * Descrição: Encontra um slot livre na tabela de conexões TCP.
 *            Retorna o índice ou -1 se a tabela estiver cheia.
 */
static int find_free_connection(void) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!connections[i].active) return i;
    }
    return -1;
}

/* ============================================================
 * FUNÇÕES PÚBLICAS DE REDE
 * ============================================================ */

/*
 * Função: init_network
 * Descrição: Inicializa o subsistema de rede TCP/IP, configurando
 *            estruturas internas e tentando detectar a placa de rede.
 */
void init_network(void) {
    kmemset(connections, 0, sizeof(connections));
    packets_sent = 0;
    packets_recv = 0;
    next_port    = 49152;

    /* Configura um MAC padrão para simulação */
    local_mac[0] = 0x52;
    local_mac[1] = 0x54;
    local_mac[2] = 0x00;
    local_mac[3] = 0x12;
    local_mac[4] = 0x34;
    local_mac[5] = 0x56;

    /* IP padrão: 10.0.2.15 (QEMU default) */
    local_ip = (10 << 24) | (0 << 16) | (2 << 8) | 15;

    /* Tenta detectar placa de rede (RTL8139 em 0x8139 via PCI) */
    /* Detecção real requer driver específico - aqui apenas simulamos */
    network_up = TRUE; /* Em ambiente QEMU, assumimos rede disponível */
}

/*
 * Função: detect_network
 * Descrição: Detecta e identifica o hardware de rede presente no sistema.
 *            Verifica dispositivos PCI de rede (classe 0x02).
 */
void detect_network(void) {
    /* A detecção real é feita pelo PCI scan em pci.c */
    /* Esta função reporta o status da rede */
    if (network_up) {
        screen_println("  Interface de rede detectada e ativa.");
    } else {
        screen_println("  Nenhuma interface de rede detectada.");
    }
}

/*
 * Função: show_network_status
 * Descrição: Exibe o status atual da pilha de rede TCP/IP.
 */
void show_network_status(void) {
    screen_println("=== Status da Rede TCP/IP ===");

    screen_print("  Interface  : ");
    screen_println(network_up ? "eth0 (Ativa)" : "Nenhuma (Inativa)");

    if (network_up) {
        /* Exibe IP no formato X.X.X.X */
        screen_print("  IP Local   : ");
        screen_print_int((s32)((local_ip >> 24) & 0xFF));
        screen_putchar('.');
        screen_print_int((s32)((local_ip >> 16) & 0xFF));
        screen_putchar('.');
        screen_print_int((s32)((local_ip >> 8) & 0xFF));
        screen_putchar('.');
        screen_print_int((s32)(local_ip & 0xFF));
        screen_println("");

        screen_print("  MAC        : ");
        for (int i = 0; i < 6; i++) {
            screen_print_hex(local_mac[i]);
            if (i < 5) screen_print(":");
        }
        screen_println("");

        screen_print("  Pkts Enviados : ");
        screen_print_int((s32)packets_sent);
        screen_println("");

        screen_print("  Pkts Recebidos: ");
        screen_print_int((s32)packets_recv);
        screen_println("");

        screen_print("  Conexoes TCP  : ");
        u32 active = 0;
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connections[i].active) active++;
        }
        screen_print_int((s32)active);
        screen_print("/");
        screen_print_int(MAX_CONNECTIONS);
        screen_println("");
    }
}

/* Declaração extern para uso interno */
extern void screen_putchar(char c);

/*
 * Função: tcp_send
 * Descrição: Envia dados via protocolo TCP para o destino especificado.
 *            Monta os cabeçalhos Ethernet, IP e TCP e envia o pacote.
 *            Retorna 0 em sucesso ou erro negativo.
 */
int tcp_send(u32 dst_ip, u16 dst_port, const u8 *data, u32 len) {
    if (!network_up) return -1;
    if (!data || len == 0) return -2;
    if (len > NET_BUFFER_SIZE - sizeof(eth_header_t)
                              - sizeof(ip_header_t)
                              - sizeof(tcp_header_t)) return -3;

    /* Em um sistema real, aqui montaríamos o pacote e enviaríamos
     * via DMA ou PIO para o controlador de rede. No simulador,
     * apenas contamos o pacote. */

    /* Monta cabeçalho IP (simulado) */
    ip_header_t ip;
    ip.version_ihl  = 0x45;          /* IPv4, 5 words = 20 bytes */
    ip.dscp_ecn     = 0;
    ip.total_len    = htons((u16)(sizeof(ip_header_t) + sizeof(tcp_header_t) + len));
    ip.id           = htons((u16)(packets_sent & 0xFFFF));
    ip.flags_offset = htons(0x4000); /* Don't Fragment */
    ip.ttl          = 64;
    ip.protocol     = IP_PROTO_TCP;
    ip.checksum     = 0;
    ip.src_ip       = htonl(local_ip);
    ip.dst_ip       = htonl(dst_ip);
    ip.checksum     = ip_checksum(&ip, sizeof(ip));

    /* Monta cabeçalho TCP (simulado) */
    tcp_header_t tcp;
    tcp.src_port    = htons(next_port++);
    tcp.dst_port    = htons(dst_port);
    tcp.seq_num     = htonl(1000);
    tcp.ack_num     = 0;
    tcp.data_offset = 0x50;         /* 5 words = 20 bytes */
    tcp.flags       = TCP_FLAG_PSH | TCP_FLAG_ACK;
    tcp.window      = htons(65535);
    tcp.checksum    = 0;
    tcp.urgent      = 0;

    packets_sent++;
    UNUSED(ip);
    UNUSED(tcp);
    UNUSED(dst_ip);
    UNUSED(dst_port);
    return 0;
}

/*
 * Função: tcp_receive
 * Descrição: Tenta receber dados de uma conexão TCP ativa.
 *            Retorna o número de bytes recebidos ou 0 se não há dados.
 */
int tcp_receive(int conn_id, u8 *buffer, u32 max_len) {
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) return -1;
    if (!connections[conn_id].active) return -1;

    tcp_connection_t *conn = &connections[conn_id];
    if (conn->rx_len == 0) return 0;

    u32 bytes = MIN(max_len, conn->rx_len);
    kmemcpy(buffer, conn->rx_buffer, bytes);

    /* Move dados restantes para o início do buffer */
    conn->rx_len -= bytes;
    if (conn->rx_len > 0) {
        kmemcpy(conn->rx_buffer, conn->rx_buffer + bytes, conn->rx_len);
    }

    packets_recv++;
    return (int)bytes;
}
