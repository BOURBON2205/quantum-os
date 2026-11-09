/* main.c - kmain: inisialisasi semua subsistem lalu jalankan shell */
#include "kernel.h"

void kmain(void) {
    console_init();
    interrupts_init();
    mem_init();
    threads_init();
    fs_init();
    banner();
        console_color(10);
    kprintf("[boot] GDT/IDT/PIC/PIT ok | heap ok | scheduler ok | ramfs ok\n\n");
    console_color(15);
    __asm__ volatile("sti");
    shell_run();
}
