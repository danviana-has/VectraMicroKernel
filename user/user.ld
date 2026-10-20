ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        *crt0.o(.text)
        *(.text._start)
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
