/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);

_Min_Heap_Size =  16 * 1024; /* required amount of heap */

MEMORY
{
  FLASH       (rx)    : ORIGIN = 0x08000000, LENGTH = 1024K
  DTCMRAM     (rw)    : ORIGIN = 0x20000000, LENGTH =  128K
  AXISRAM     (rw)    : ORIGIN = 0x24000000, LENGTH =  128K
  SRAM1_D2    (rw)    : ORIGIN = 0x30000000, LENGTH =   16K
  SRAM2_D2    (rw)    : ORIGIN = 0x30004000, LENGTH =   16K
}

SECTIONS
{
  /* The startup code into "FLASH" type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } > FLASH

  /* The program code and other data into "FLASH" type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
  } > FLASH

  /* Constant data into "FLASH" type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } > FLASH

  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } > FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } > FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } > FLASH

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */

  } > AXISRAM AT > FLASH

  .bss :
  {
    . = ALIGN(4);
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .; /* define a global symbol at bss start */
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .; /* define a global symbol at bss end */

    /* check whether there is enough space for heap memory */
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = ALIGN(8);
  } > AXISRAM

  /* Used by _sbrk to protect from out of memory region fault */
  _max_heap = ORIGIN(AXISRAM) + LENGTH(AXISRAM);

  .ethdma (NOLOAD) :
  {
    *(.RxDecripSection)
    . = ALIGN(0x2000);
    *(.TxDecripSection)
  } > SRAM1_D2

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    *(.init_array.*)
    *(.init_array*)
    *(.fini_array.*)
    *(.fini_array*)
    *(.ARM.attributes)
  }
}
