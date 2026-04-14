/*
 * keyboard.c - Driver de teclado PS/2
 * Descrição: Driver para leitura de entrada do teclado PS/2 via
 *            porta de I/O 0x60 (teclado) e 0x64 (status/controle).
 *            Implementa mapeamento de scancodes para caracteres ASCII.
 */

#include "../include/kernel.h"

/* ============================================================
 * CONSTANTES DO TECLADO PS/2
 * ============================================================ */
#define KB_DATA_PORT    0x60    /* Porta de dados do teclado */
#define KB_STATUS_PORT  0x64    /* Porta de status do controlador */
#define KB_STATUS_OBF   0x01    /* Output Buffer Full - dado disponível */
#define KB_STATUS_IBF   0x02    /* Input Buffer Full - ocupado */

/* Scancodes especiais */
#define SC_LSHIFT       0x2A    /* Shift esquerdo pressionado */
#define SC_RSHIFT       0x36    /* Shift direito pressionado */
#define SC_LSHIFT_R     0xAA    /* Shift esquerdo liberado */
#define SC_RSHIFT_R     0xB6    /* Shift direito liberado */
#define SC_CAPS         0x3A    /* Caps Lock */
#define SC_CTRL         0x1D    /* Control */
#define SC_ALT          0x38    /* Alt */
#define SC_BACKSPACE    0x0E    /* Backspace */
#define SC_ENTER        0x1C    /* Enter */
#define SC_TAB          0x0F    /* Tab */
#define SC_RELEASE      0x80    /* Bit indicador de liberação de tecla */

/* ============================================================
 * TABELA DE SCANCODES (Set 1 - padrão US QWERTY)
 * ============================================================ */

/* Mapa de caracteres sem Shift */
static const char scancode_map[] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', /* 0x08-0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  /* 0x10-0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  /* 0x20-0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38-0x3F */
};

/* Mapa de caracteres com Shift */
static const char scancode_shift_map[] = {
    0,    0,    '!',  '@',  '#',  '$',  '%',  '^',  /* 0x00-0x07 */
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', /* 0x08-0x0F */
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  /* 0x10-0x17 */
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  /* 0x18-0x1F */
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  /* 0x20-0x27 */
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  /* 0x28-0x2F */
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38-0x3F */
};

/* ============================================================
 * BUFFER DE ENTRADA DO TECLADO
 * ============================================================ */
#define KB_BUFFER_SIZE  256

static char kb_buffer[KB_BUFFER_SIZE];
static u32  kb_buf_head = 0;
static u32  kb_buf_tail = 0;
static bool shift_pressed = FALSE;
static bool caps_lock     = FALSE;
static bool ctrl_pressed  = FALSE;

/* ============================================================
 * FUNÇÕES INTERNAS
 * ============================================================ */

/*
 * Função: kb_buffer_push
 * Descrição: Insere um caractere no buffer circular de entrada do teclado.
 */
static void kb_buffer_push(char c) {
    u32 next = (kb_buf_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_buf_tail) {
        kb_buffer[kb_buf_head] = c;
        kb_buf_head = next;
    }
}

/*
 * Função: kb_buffer_pop
 * Descrição: Remove e retorna um caractere do buffer de entrada.
 *            Retorna 0 se o buffer estiver vazio.
 */
static char kb_buffer_pop(void) {
    if (kb_buf_head == kb_buf_tail) return 0;
    char c = kb_buffer[kb_buf_tail];
    kb_buf_tail = (kb_buf_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

/*
 * Função: kb_handle_scancode
 * Descrição: Processa um scancode recebido do teclado e converte
 *            para o caractere ASCII correspondente.
 */
static void kb_handle_scancode(u8 scancode) {
    bool released = (scancode & SC_RELEASE) != 0;
    u8   key      = scancode & ~SC_RELEASE;

    /* Teclas modificadoras */
    if (key == SC_LSHIFT || key == SC_RSHIFT) {
        shift_pressed = !released;
        return;
    }
    if (key == SC_CTRL) {
        ctrl_pressed = !released;
        return;
    }
    if (key == SC_CAPS && !released) {
        caps_lock = !caps_lock;
        return;
    }

    /* Ignora eventos de liberação de outras teclas */
    if (released) return;

    /* Converte scancode para caractere */
    if (key >= sizeof(scancode_map)) return;

    char c;
    bool use_upper = shift_pressed ^ caps_lock;

    if (use_upper && key < (u8)sizeof(scancode_shift_map)) {
        c = scancode_shift_map[key];
    } else {
        c = scancode_map[key];
    }

    if (c != 0) {
        kb_buffer_push(c);
    }
}

/* ============================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_keyboard
 * Descrição: Inicializa o driver do teclado PS/2, limpando o buffer
 *            de entrada e preparando as estruturas de estado.
 */
void init_keyboard(void) {
    kb_buf_head   = 0;
    kb_buf_tail   = 0;
    shift_pressed = FALSE;
    caps_lock     = FALSE;
    ctrl_pressed  = FALSE;

    /* Limpa qualquer dado pendente na porta do teclado */
    while (inb(KB_STATUS_PORT) & KB_STATUS_OBF) {
        inb(KB_DATA_PORT);
    }
}

/*
 * Função: kb_poll
 * Descrição: Verifica se há dados disponíveis na porta do teclado e
 *            processa o scancode recebido (modo polling sem interrupção).
 *            Deve ser chamada periodicamente no loop principal.
 */
void kb_poll(void) {
    if (inb(KB_STATUS_PORT) & KB_STATUS_OBF) {
        u8 scancode = inb(KB_DATA_PORT);
        kb_handle_scancode(scancode);
    }
}

/*
 * Função: kb_getchar
 * Descrição: Aguarda e retorna o próximo caractere digitado pelo usuário.
 *            Bloqueia até que um caractere esteja disponível no buffer.
 */
char kb_getchar(void) {
    char c;
    do {
        kb_poll();
        c = kb_buffer_pop();
    } while (c == 0);
    return c;
}

/*
 * Função: kb_getchar_noblock
 * Descrição: Retorna o próximo caractere do buffer sem bloquear.
 *            Retorna 0 se não houver caractere disponível.
 */
char kb_getchar_noblock(void) {
    kb_poll();
    return kb_buffer_pop();
}

/*
 * Função: kb_readline
 * Descrição: Lê uma linha completa do teclado (até Enter ou tamanho máximo),
 *            com suporte a Backspace para edição. Exibe os caracteres na tela.
 */
void kb_readline(char *buffer, u32 max_len) {
    extern void screen_putchar(char c);
    u32 pos = 0;

    while (pos < max_len - 1) {
        char c = kb_getchar();

        if (c == '\n' || c == '\r') {
            screen_putchar('\n');
            break;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                screen_putchar('\b');
            }
        } else if (c >= 32 && c < 127) {
            buffer[pos++] = c;
            screen_putchar(c);
        }
    }

    buffer[pos] = '\0';
}
