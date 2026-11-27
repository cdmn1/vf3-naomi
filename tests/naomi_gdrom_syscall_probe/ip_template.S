    .section .rodata
    .balign 4
    .global _binary_ip_template_bin_start
_binary_ip_template_bin_start:
    .incbin "ip_template.bin"
    .global _binary_ip_template_bin_end
_binary_ip_template_bin_end:
