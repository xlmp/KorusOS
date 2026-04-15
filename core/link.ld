ENTRY(_start)

SECTIONS {
    . = 0xFFFFFFFF80100000;   /* Endereço virtual alto (kernel space x64) */

    .text ALIGN(4096) : {
        *(.multiboot2)
        *(.multiboot)
        *(.text)
    }

    .rodata ALIGN(4096) : {
        *(.rodata*)
    }

    .data ALIGN(4096) : {
        *(.data)
    }

    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
    }
}