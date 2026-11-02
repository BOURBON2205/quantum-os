/* isr.S - stub interrupt (0-31 exception, 32 = timer) + context switch */
.section .rodata
.global isr_table
isr_table:

.macro ISR_NOERR n
.section .text
isr\n:
    pushl $0
    pushl $\n
    jmp isr_common
.pushsection .rodata
    .long isr\n
.popsection
.endm

.macro ISR_ERR n
.section .text
isr\n:
    pushl $\n
    jmp isr_common
.pushsection .rodata
    .long isr\n
.popsection
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31
ISR_NOERR 32

.section .text
isr_common:
    pusha
    cld
    push %esp
    call isr_handler
    add $4, %esp
    popa
    add $8, %esp
    iret

/* void switch_context(uint32_t *old_esp, uint32_t new_esp) */
.global switch_context
switch_context:
    mov 4(%esp), %eax
    mov 8(%esp), %edx
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov %esp, (%eax)
    mov %edx, %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
