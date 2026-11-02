/* boot.S - entry point + Multiboot header (dimuat GRUB / loader multiboot QEMU) */
.set MAGIC,    0x1BADB002
.set FLAGS,    0
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text
.global _start
_start:
    cli
    mov $stack_top, %esp
    xor %ebp, %ebp
    call kmain
1:  hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
