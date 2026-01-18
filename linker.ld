ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text : AT(0x1000)
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .init_array :
    {
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;
    }

    .ctors :
    {
        __ctors_start = .;
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        __ctors_end = .;
    }

    .bss :
    {
        *(.bss)
        *(COMMON)
    }
}
