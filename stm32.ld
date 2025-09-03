ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);
MEMORY
{
  RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH (rx) : ORIGIN = 0x8000000, LENGTH = 1024K
}

SECTIONS {
    /* Таблица векторов прерываний */
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))         /* Секция с векторами */
        . = ALIGN(4);
    } >FLASH

    /* Код и константы */
    .text : {
        *(.text)                  /* Код */
        *(.text*)                 /* Код функций */
        *(.rodata)                /* Константы */
        *(.rodata*)               /* Константы */
        . = ALIGN(4);
        _etext = .;               /* Конец секции кода */
    } >FLASH

    _sidata = .;

    /* Инициализированные данные (копируются из FLASH в RAM) */
    .data : AT (_sidata) {
        . = ALIGN(4);
        _sdata = .;               /* Начао .data в RAM */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;               /* Конец .data в RAM */
    } >RAM

    /* Неинициализированные данные (BSS) */
    .bss : {
        . = ALIGN(4);
        _sbss = .;                /* Начало BSS */
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;                /* Конец BSS */
    } >RAM
}
