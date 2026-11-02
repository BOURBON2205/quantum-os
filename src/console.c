/* console.c - output VGA + serial COM1, input keyboard PS/2 + serial, kprintf, readline */
#include "kernel.h"

#define COM1 0x3F8
#define VGA_W 80
#define VGA_H 25

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static int cx, cy;

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);   /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) ;
    outb(COM1, (u8)c);
}

static void vga_scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++) vga[i] = vga[i + VGA_W];
    for (int i = 0; i < VGA_W; i++) vga[VGA_W * (VGA_H - 1) + i] = 0x0F00 | ' ';
    cy = VGA_H - 1;
}

static void vga_putc(char c) {
    if (c == '\r') return;
    if (c == '\n') { cx = 0; cy++; }
    else if (c == '\b') { if (cx > 0) { cx--; vga[cy * VGA_W + cx] = 0x0F00 | ' '; } }
    else { vga[cy * VGA_W + cx] = 0x0F00 | (u8)c; if (++cx >= VGA_W) { cx = 0; cy++; } }
    if (cy >= VGA_H) vga_scroll();
}

void console_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = 0x0F00 | ' ';
    cx = cy = 0;
    kputs("\033[2J\033[H");     /* ANSI clear untuk terminal serial */
}

void console_init(void) {
    serial_init();
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = 0x0F00 | ' ';
    cx = cy = 0;
}

void kputc(char c) {
    if (c == '\033') { serial_putc(c); return; }   /* escape ANSI hanya ke serial */
    if (c == '\n') serial_putc('\r');
    if (c == '\b') { serial_putc('\b'); serial_putc(' '); serial_putc('\b'); }
    else serial_putc(c);
    vga_putc(c);
}

void kputs(const char *s) { while (*s) kputc(*s++); }

static void put_num(u32 v, u32 base, int width, char pad) {
    char b[16]; int n = 0;
    if (v == 0) b[n++] = '0';
    while (v) { b[n++] = "0123456789abcdef"[v % base]; v /= base; }
    while (n < width && n < 15) b[n++] = pad;
    while (n) kputc(b[--n]);
}

void kprintf(const char *f, ...) {
    u32 fl = irq_save();             /* satu baris output = atomik */
    va_list ap; va_start(ap, f);
    for (; *f; f++) {
        if (*f != '%') { kputc(*f); continue; }
        f++;
        char pad = ' '; int w = 0;
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') w = w * 10 + (*f++ - '0');
        switch (*f) {
        case 'd': { int v = va_arg(ap, int); if (v < 0) { kputc('-'); v = -v; } put_num((u32)v, 10, w, pad); break; }
        case 'u': put_num(va_arg(ap, u32), 10, w, pad); break;
        case 'x': put_num(va_arg(ap, u32), 16, w, pad); break;
        case 'p': kputs("0x"); put_num(va_arg(ap, u32), 16, 8, '0'); break;
        case 's': { const char *s = va_arg(ap, const char *); int l = s ? (int)strlen(s) : 6;
                    for (int i = l; i < w; i++) kputc(' ');
                    kputs(s ? s : "(null)"); break; }
        case 'c': kputc((char)va_arg(ap, int)); break;
        case '%': kputc('%'); break;
        default:  kputc('%'); kputc(*f); break;
        }
    }
    va_end(ap);
    irq_restore(fl);
}

/* ---------- input ---------- */
static const char km[]  = "\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char kms[] = "\0\033" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";

static int kbd_getc_nb(void) {
    static int shift, ext;
    if (!(inb(0x64) & 1)) return -1;
    u8 sc = inb(0x60);
    if (sc == 0xE0) { ext = 1; return -1; }
    if (ext) { ext = 0; return -1; }
    if (sc == 0x2A || sc == 0x36) { shift = 1; return -1; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return -1; }
    if (sc & 0x80) return -1;
    if (sc >= sizeof(km) - 1) return -1;
    char c = shift ? kms[sc] : km[sc];
    return c ? (u8)c : -1;
}

int console_getc_nb(void) {
    if (inb(COM1 + 5) & 1) {
        int c = inb(COM1);
        if (c == '\r') c = '\n';
        if (c == 0x7F) c = '\b';
        return c;
    }
    return kbd_getc_nb();
}

int readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        int c = console_getc_nb();
        if (c < 0) {
            thread_yield();
            if (!others_ready()) __asm__ volatile("sti; hlt");
            continue;
        }
        if (c == '\n') { kputc('\n'); buf[n] = 0; return n; }
        if (c == '\b') { if (n > 0) { n--; kputc('\b'); } continue; }
        if (c >= 32 && c < 127 && n < max - 1) { buf[n++] = (char)c; kputc((char)c); }
    }
}
