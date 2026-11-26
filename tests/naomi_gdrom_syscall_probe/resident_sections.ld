SECTIONS
{
  .vf3_resident 0x8D000000 : ALIGN(32)
  {
    __vf3_resident_start = .;
    KEEP(*(.vf3_resident_text))
    KEEP(*(.vf3_resident_text.*))
    KEEP(*(.vf3_resident_rodata))
    KEEP(*(.vf3_resident_rodata.*))
    KEEP(*(.vf3_resident_data))
    KEEP(*(.vf3_resident_data.*))
    . = ALIGN(32);
    __vf3_resident_end = .;
  }
}
INSERT AFTER .ocram;
