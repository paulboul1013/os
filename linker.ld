ENTRY(_start)
SECTIONS
{
    . = 0x8000;
    .text : { boot/kernel_entry.o(.text) *(EXCLUDE_FILE (user/*.o) .text .text.*) }
    .rodata : { *(EXCLUDE_FILE (user/*.o) .rodata .rodata.*) }
    .data : { *(EXCLUDE_FILE (user/*.o) .data .data.*) }
    .init_array : {
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*))) KEEP (*(.init_array))
        KEEP (*(SORT(.ctors.*))) KEEP (*(.ctors))
        __init_array_end = .;
    }
    __ctors_start = .; __ctors_end = .;
    . = ALIGN(4096);
    __user_text_start = .;
    .user_text : { user/*.o(.text .text.*) . = ALIGN(4096); }
    __user_text_end = .;
    __user_rodata_start = .;
    .user_rodata : { user/*.o(.rodata .rodata.*) . = ALIGN(4096); }
    __user_rodata_end = .;
    __user_data_start = .;
    .user_data : { user/*.o(.data .data.*) . = ALIGN(4096); }
    __user_data_file_end = .;
    .user_bss (NOLOAD) : { user/*.o(.bss .bss.* COMMON) . = ALIGN(4096); }
    __user_data_end = .;
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*) *(COMMON)
        __bss_end = .;
    }
    ASSERT(__bss_end < 0x80000, "kernel overlaps heap limit")
    /DISCARD/ : { *(.eh_frame*) *(.comment) }
}
