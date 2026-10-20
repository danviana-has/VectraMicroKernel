/* ==============================================================================
 * Vectra OS - Linker Script (x86 32-bit ELF / kernel.akl)
 * ============================================================================== */

ENTRY(_start)

SECTIONS
{
    /* O kernel eh carregado em 1 MiB (0x100000), endereco padrao de carga Multiboot */
    . = 0x00100000;

    _kernel_start = .;

    /* 1. Secao Multiboot (obrigatoriamente dentro dos primeiros 8 KiB do binario) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    }

    /* 2. Dados somente leitura (constantes e strings literais) */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    /* 3. Dados inicializados (variaveis globais e estaticas inicializadas) */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    /* 4. Dados nao inicializados (BSS e Common) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4K);
    _kernel_end = .;
}
