/* interrupts.c - GDT, IDT, PIC, PIT (timer 100 Hz), handler exception */
#include "kernel.h"

struct gdt_entry { u16 limit_lo, base_lo; u8 base_mid, access, gran, base_hi; } __attribute__((packed));
struct dt_ptr    { u16 limit; u32 base; } __attribute__((packed));
struct idt_entry { u16 off_lo, sel; u8 zero, flags; u16 off_hi; } __attribute__((packed));
struct regs { u32 edi, esi, ebp, esp_d, ebx, edx, ecx, eax, vec, err, eip, cs, eflags; };

static struct gdt_entry gdt[3];
static struct idt_entry idt[256];
extern u32 isr_table[];
volatile u32 ticks;

static void gdt_set(int i, u32 base, u32 lim, u8 acc, u8 gran) {
    gdt[i].base_lo = base & 0xFFFF; gdt[i].base_mid = (base >> 16) & 0xFF; gdt[i].base_hi = base >> 24;
    gdt[i].limit_lo = lim & 0xFFFF; gdt[i].gran = ((lim >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access = acc;
}

static void idt_set(int i, u32 h) {
    idt[i].off_lo = h & 0xFFFF; idt[i].off_hi = h >> 16;
    idt[i].sel = 0x08; idt[i].zero = 0; idt[i].flags = 0x8E;   /* interrupt gate ring 0 */
}

void interrupts_init(void) {
    /* GDT flat: null, code (0x08), data (0x10) */
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    struct dt_ptr gp = { sizeof(gdt) - 1, (u32)gdt };
    __asm__ volatile("lgdt %0\n mov $0x10,%%ax\n mov %%ax,%%ds\n mov %%ax,%%es\n"
                     " mov %%ax,%%fs\n mov %%ax,%%gs\n mov %%ax,%%ss\n ljmp $0x08,$1f\n1:"
                     :: "m"(gp) : "ax", "memory");
    /* IDT */
    for (int i = 0; i < 33; i++) idt_set(i, isr_table[i]);
    struct dt_ptr ip = { sizeof(idt) - 1, (u32)idt };
    __asm__ volatile("lidt %0" :: "m"(ip));
    /* PIC remap: IRQ0-7 -> vektor 32-39, hanya IRQ0 (timer) aktif */
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFE); outb(0xA1, 0xFF);
    /* PIT channel 0, mode 3, 100 Hz */
    u32 div = 1193182 / TICK_HZ;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, div >> 8);
}

static const char *exc_name[] = {
    "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound Range", "Invalid Opcode",
    "Device N/A", "Double Fault", "Coproc Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection", "Page Fault", "Reserved", "x87 FPU", "Alignment Check",
    "Machine Check", "SIMD FP"
};

void isr_handler(struct regs *r) {
    if (r->vec == 32) {
        outb(0x20, 0x20);            /* EOI dulu, supaya tick berikutnya tidak hilang saat context switch */
        timer_tick();
        return;
    }
    kprintf("\n*** KERNEL PANIC: exception %d (%s) err=%x eip=%p ***\n", r->vec,
            r->vec < 20 ? exc_name[r->vec] : "?", r->err, r->eip);
    for (;;) __asm__ volatile("cli; hlt");
}
