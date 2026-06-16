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

    /* 丟棄 .eh_frame（GCC -g、例外處理框架），裸機 OS 不需要，
       若不丟棄，它會插入在 .data 前並把後面 section 的 VMA 推偏，
       導致 __ctors_start 符號與實際資料位置不一致。 */
    /DISCARD/ : { *(.eh_frame) *(.eh_frame_hdr) }

    .data :
    {
        *(.data)
    }

    /* 把 .ctors 也合入 init_array，讓所有 constructors 由同一個符號範圍管理 */
    .init_array :
    {
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        __init_array_end = .;
    }

    /* 保留空的 .ctors section 定義以向後相容；start==end 表示空 */
    .ctors :
    {
        __ctors_start = .;
        __ctors_end = .;
    }

    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}
