/*
 * screen.c - Driver de vídeo VGA em modo texto
 * Descrição: Driver para saída de texto no terminal usando o buffer VGA
 *            de modo texto padrão (80x25 caracteres) presente em hardware
 *            x86 compatível com PC.
 *
 * O buffer VGA de texto fica mapeado no endereço físico 0xB8000.
 * Cada célula do buffer é composta por 2 bytes: caractere + atributo de cor.
 */

#include "../include/kernel.h"

/* ============================================================
 * CONSTANTES VGA
 * ============================================================ */
#define VGA_WIDTH       80      /* Colunas do terminal */
#define VGA_HEIGHT      25      /* Linhas do terminal */
#define VGA_MEMORY      ((u16 *)0xB8000)  /* Endereço do buffer VGA */

/* Portas do controlador VGA para posição do cursor */
#define VGA_CTRL_PORT   0x3D4
#define VGA_DATA_PORT   0x3D5
#define VGA_CURSOR_HIGH 14
#define VGA_CURSOR_LOW  15

/* ============================================================
 * VARIÁVEIS INTERNAS DO DRIVER
 * ============================================================ */
static u32 cursor_x  = 0;  /* Coluna atual do cursor */
static u32 cursor_y  = 0;  /* Linha atual do cursor */
static u8  cur_color = 0x07; /* Atributo atual (cinza claro / fundo preto) */

/* ============================================================
 * FUNÇÕES AUXILIARES INTERNAS
 * ============================================================ */

/*
 * Função: make_entry
 * Descrição: Combina caractere e atributo de cor em uma entrada VGA de 16 bits.
 */
static inline u16 make_entry(char c, u8 color) {
    return (u16)c | ((u16)color << 8);
}

/*
 * Função: update_cursor
 * Descrição: Atualiza a posição do cursor hardware do VGA via portas de I/O.
 */
static void update_cursor(void) {
    u32 pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(VGA_CTRL_PORT, VGA_CURSOR_HIGH);
    outb(VGA_DATA_PORT, (u8)((pos >> 8) & 0xFF));
    outb(VGA_CTRL_PORT, VGA_CURSOR_LOW);
    outb(VGA_DATA_PORT, (u8)(pos & 0xFF));
}

/*
 * Função: scroll_up
 * Descrição: Move todas as linhas do terminal uma posição para cima,
 *            limpando a última linha para nova entrada de texto.
 */
static void scroll_up(void) {
    /* Copia todas as linhas uma posição acima */
    for (u32 y = 1; y < VGA_HEIGHT; y++) {
        for (u32 x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    /* Limpa a última linha */
    for (u32 x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            make_entry(' ', cur_color);
    }

    if (cursor_y > 0) cursor_y--;
}

/* ============================================================
 * FUNÇÕES PÚBLICAS DO DRIVER
 * ============================================================ */

/*
 * Função: init_screen
 * Descrição: Inicializa o driver de vídeo VGA, limpa a tela e
 *            posiciona o cursor no canto superior esquerdo.
 */
void init_screen(void) {
    cursor_x  = 0;
    cursor_y  = 0;
    cur_color = 0x07; /* Cinza claro no fundo preto */
    /* A limpeza efetiva ocorre em screen_clear() */
}

/*
 * Função: screen_clear
 * Descrição: Limpa toda a tela do terminal preenchendo o buffer VGA
 *            com espaços e reposiciona o cursor no início.
 */
void screen_clear(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = make_entry(' ', cur_color);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

/*
 * Função: screen_set_color
 * Descrição: Define a cor atual do texto (foreground) e do fundo (background).
 *            Cores VGA: 0=preto, 1=azul, 2=verde, 3=ciano, 4=vermelho,
 *            5=magenta, 6=marrom, 7=cinza, 8-15=versões brilhantes.
 */
void screen_set_color(u8 fg, u8 bg) {
    cur_color = (bg << 4) | (fg & 0x0F);
}

/*
 * Função: screen_putchar
 * Descrição: Exibe um único caractere na posição atual do cursor,
 *            tratando caracteres especiais como '\n', '\r' e '\b'.
 */
void screen_putchar(char c) {
    if (c == '\n') {
        /* Nova linha */
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        /* Retorno de carro */
        cursor_x = 0;
    } else if (c == '\b') {
        /* Backspace */
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] =
                make_entry(' ', cur_color);
        }
    } else if (c == '\t') {
        /* Tab - avança para próxima coluna múltipla de 8 */
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    } else {
        /* Caractere normal */
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] =
            make_entry(c, cur_color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    /* Rola a tela se chegou ao final */
    if (cursor_y >= VGA_HEIGHT) {
        scroll_up();
    }

    update_cursor();
}

/*
 * Função: screen_print
 * Descrição: Exibe uma string no terminal caractere por caractere.
 */
void screen_print(const char *str) {
    if (!str) return;
    while (*str) {
        screen_putchar(*str++);
    }
}

/*
 * Função: screen_println
 * Descrição: Exibe uma string seguida de nova linha no terminal.
 */
void screen_println(const char *str) {
    screen_print(str);
    screen_putchar('\n');
}

/*
 * Função: screen_print_hex
 * Descrição: Exibe um número inteiro sem sinal em formato hexadecimal
 *            prefixado com "0x" no terminal.
 */
void screen_print_hex(uptr val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[19];
    buf[0]  = '0';
    buf[1]  = 'x';
    buf[18] = '\0';

    for (int i = 17; i >= 2; i--) {
        buf[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    screen_print(buf);
}

/*
 * Função: screen_print_int
 * Descrição: Exibe um número inteiro com sinal em formato decimal no terminal.
 */
void screen_print_int(s32 val) {
    if (val < 0) {
        screen_putchar('-');
        val = -val;
    }

    if (val == 0) {
        screen_putchar('0');
        return;
    }

    char buf[12];
    int  i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    /* Exibe em ordem reversa */
    for (int j = i - 1; j >= 0; j--) {
        screen_putchar(buf[j]);
    }
}

/*
 * Função: screen_get_cursor
 * Descrição: Retorna a posição atual do cursor (coluna e linha).
 */
void screen_get_cursor(u32 *x, u32 *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}
