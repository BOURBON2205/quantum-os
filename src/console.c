/* console.c - VGA (dengan header tetap + kursor hardware) + serial COM1,
 *             input keyboard PS/2 + serial (termasuk tombol panah), kprintf, readline dengan edit baris */
#include "kernel.h"

#define COM1  0x3F8
#define VGA_W 80
#define VGA_H 25

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static int cx, cy;
static int scroll_top;            /* baris < scroll_top = header tetap, tidak ikut scroll */
static u8  attr = 15;             /* warna teks saat ini (putih terang) */
static int mouse_enabled, mouse_active;
static int mouse_px, mouse_py, mouse_pos, mouse_buttons;
static int mouse_click_pending, mouse_click_x, mouse_click_y;
static int mouse_drawn, mouse_cell;
static u16 mouse_saved;
static u8 mouse_packet[3];

/* ---------- serial ---------- */
static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);   /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) { while (!(inb(COM1 + 5) & 0x20)) ; outb(COM1, (u8)c); }
static void serial_puts(const char *s) { while (*s) serial_putc(*s++); }

/* ---------- VGA ---------- */
static void hw_cursor(void) {                       /* kursor berkedip mengikuti posisi ketik */
    u16 p = (u16)(cy * VGA_W + cx);
    outb(0x3D4, 0x0F); outb(0x3D5, p & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, p >> 8);
}

static int ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) return 1;
    return 0;
}
static int ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return 1;
    return 0;
}
static int mouse_send(u8 value) {
    if (!ps2_wait_write()) return 0;
    outb(0x64, 0xD4);
    if (!ps2_wait_write()) return 0;
    outb(0x60, value);
    if (!ps2_wait_read()) return 0;
    return inb(0x60) == 0xFA;
}

static void mouse_undraw(void) {
    if (!mouse_drawn) return;
    vga[mouse_cell] = mouse_saved;
    mouse_drawn = 0;
}
static void mouse_draw(void) {
    if (!mouse_enabled || !mouse_active || mouse_drawn) return;
    int x = mouse_px >> 3, y = mouse_py >> 4;
    if (x < 0 || x >= VGA_W || y < 0 || y >= VGA_H) return;
    mouse_cell = y * VGA_W + x;
    mouse_saved = vga[mouse_cell];
    vga[mouse_cell] = 0x1E00 | '>';
    mouse_drawn = 1;
}

static void mouse_packet_byte(u8 b) {
    if (mouse_pos == 0 && !(b & 0x08)) return;
    mouse_packet[mouse_pos++] = b;
    if (mouse_pos < 3) return;
    mouse_pos = 0;

    u8 flags = mouse_packet[0];
    if (flags & 0xC0) return;
    int dx = mouse_packet[1] < 128 ? mouse_packet[1] : (int)mouse_packet[1] - 256;
    int dy = mouse_packet[2] < 128 ? mouse_packet[2] : (int)mouse_packet[2] - 256;
    u8 buttons = flags & 7;

    mouse_undraw();
    mouse_active = 1;
    mouse_px += dx;
    mouse_py -= dy;
    if (mouse_px < 0) mouse_px = 0;
    if (mouse_px >= VGA_W * 8) mouse_px = VGA_W * 8 - 1;
    if (mouse_py < 0) mouse_py = 0;
    if (mouse_py >= VGA_H * 16) mouse_py = VGA_H * 16 - 1;
    if ((buttons & 1) && !(mouse_buttons & 1)) {
        mouse_click_x = mouse_px >> 3;
        mouse_click_y = mouse_py >> 4;
        mouse_click_pending = 1;
    }
    mouse_buttons = buttons;
    mouse_draw();
}

static void mouse_poll(void) {
    if (!mouse_enabled) return;
    for (int i = 0; i < 16; i++) {
        u8 status = inb(0x64);
        if (!(status & 1) || !(status & 0x20)) break;
        mouse_packet_byte(inb(0x60));
    }
}

static void mouse_init(void) {
    mouse_enabled = mouse_active = mouse_drawn = mouse_click_pending = 0;
    mouse_pos = mouse_buttons = 0;
    mouse_px = (VGA_W / 2) * 8;
    mouse_py = (VGA_H / 2) * 16;

    if (!ps2_wait_write()) return;
    outb(0x64, 0xA8);
    if (!ps2_wait_write()) return;
    outb(0x64, 0x20);
    if (!ps2_wait_read()) return;
    u8 config = inb(0x60);
    config &= (u8)~0x22;                    /* polling, without IRQ12 */
    if (!ps2_wait_write()) return;
    outb(0x64, 0x60);
    if (!ps2_wait_write()) return;
    outb(0x60, config);
    if (!mouse_send(0xF6)) return;
    if (!mouse_send(0xF4)) return;
    mouse_enabled = 1;
}

static void vga_scroll(void) {
    for (int i = scroll_top * VGA_W; i < VGA_W * (VGA_H - 1); i++) vga[i] = vga[i + VGA_W];
    for (int i = 0; i < VGA_W; i++) vga[VGA_W * (VGA_H - 1) + i] = 0x0700 | ' ';
    cy = VGA_H - 1;
}

static void vga_putc(char c) {
    if (c == '\r') return;
    mouse_undraw();
    if (c == '\n') { cx = 0; cy++; }
    else if (c == '\b') { if (cx > 0) { cx--; vga[cy * VGA_W + cx] = ((u16)attr << 8) | ' '; } }
    else { vga[cy * VGA_W + cx] = ((u16)attr << 8) | (u8)c; if (++cx >= VGA_W) { cx = 0; cy++; } }
    if (cy >= VGA_H) vga_scroll();
    mouse_draw();
    hw_cursor();
}

void console_color(u8 c) {
    static const u8 ansi[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };   /* urutan warna VGA -> ANSI */
    attr = c & 15;
    char b[8] = { 27, '[', (char)((attr < 8 ? '3' : '9')), (char)('0' + ansi[attr & 7]), 'm', 0 };
    serial_puts(b);
}

void console_set_header_end(void) { scroll_top = cy; }

void console_get_cursor(int *x, int *y) {
    if (x) *x = cx;
    if (y) *y = cy;
}

int console_get_mouse_click(int *x, int *y) {
    mouse_poll();
    if (!mouse_click_pending) return 0;
    if (x) *x = mouse_click_x;
    if (y) *y = mouse_click_y;
    mouse_click_pending = 0;
    return 1;
}

void console_clear(void) {
    mouse_undraw();
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = 0x0700 | ' ';
    cx = cy = 0; scroll_top = 0;
    hw_cursor();
    serial_puts("\033[2J\033[H");
    banner();                                       /* header (logo) digambar ulang -> tidak hilang */
    mouse_draw();
}

void console_init(void) {
    serial_init();
    outb(0x3D4, 0x0A); outb(0x3D5, 0x0D);           /* bentuk kursor: garis bawah tebal */
    outb(0x3D4, 0x0B); outb(0x3D5, 0x0F);
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = 0x0700 | ' ';
    cx = cy = 0; scroll_top = 0;
    hw_cursor();
    serial_puts("\033[0m\033[2J\033[H");
    mouse_init();
}

void kputc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
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
    u32 fl = irq_save();             /* satu panggilan output = atomik */
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

/* ---------- input: keyboard PS/2 ---------- */
static const char km[]  = "\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char kms[] = "\0\033" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";

static int kbd_getc_nb(void) {
    static int shift, ext;
    u8 status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return -1;
    u8 sc = inb(0x60);
    if (sc == 0xE0) { ext = 1; return -1; }
    int rel = sc & 0x80; u8 code = sc & 0x7F;
    if (ext) {                                       /* tombol extended: panah, home, end, del */
        ext = 0;
        if (rel) return -1;
        switch (code) {
        case 0x4B: return KEY_LEFT;  case 0x4D: return KEY_RIGHT;
        case 0x48: return KEY_UP;    case 0x50: return KEY_DOWN;
        case 0x47: return KEY_HOME;  case 0x4F: return KEY_END;
        case 0x53: return KEY_DEL;   case 0x1C: return '\n';
        }
        return -1;
    }
    if (code == 0x2A || code == 0x36) { shift = !rel; return -1; }
    if (rel) return -1;
    if (code >= sizeof(km) - 1) return -1;
    char c = shift ? kms[code] : km[code];
    return c ? (u8)c : -1;
}

/* ---------- input: serial (menerjemahkan urutan ANSI ESC [ A/B/C/D/H/F/3~) ---------- */
static int serial_getc_nb(void) {
    static int st, num;
    while (inb(COM1 + 5) & 1) {
        int c = inb(COM1);
        if (st == 0) {
            if (c == 0x1B) { st = 1; continue; }
            if (c == '\r') return '\n';
            if (c == 0x7F) return '\b';
            return c;
        }
        if (st == 1) { if (c == '[' || c == 'O') { st = 2; num = 0; } else st = 0; continue; }
        if (c >= '0' && c <= '9') { num = num * 10 + (c - '0'); continue; }
        st = 0;
        switch (c) {
        case 'A': return KEY_UP;   case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT; case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME; case 'F': return KEY_END;
        case '~': if (num == 3) return KEY_DEL;
                  if (num == 1 || num == 7) return KEY_HOME;
                  if (num == 4 || num == 8) return KEY_END;
                  break;
        }
    }
    return -1;
}

int console_getc_nb(void) {
    int c = serial_getc_nb();
    if (c >= 0) return c;
    mouse_poll();
    return kbd_getc_nb();
}

/* ---------- readline dengan edit di posisi kursor + riwayat ---------- */
#define HIST_N 8
static char hist[HIST_N][128];
static int  hist_cnt;

static void back(int n) {                            /* geser kursor n kolom ke kiri tanpa menghapus */
    if (n <= 0) return;
    int lin = cy * VGA_W + cx - n;
    if (lin < scroll_top * VGA_W) lin = scroll_top * VGA_W;
    cy = lin / VGA_W; cx = lin % VGA_W; hw_cursor();
    for (int i = 0; i < n; i++) serial_putc('\b');
}
static void show(const char *s, int len) { for (int i = 0; i < len; i++) kputc(s[i]); }

static void replace(char *buf, int *n, int *pos, const char *s) {   /* ganti seluruh isi baris */
    back(*pos);
    for (int i = 0; i < *n; i++) kputc(' ');
    back(*n);
    int l = (int)strlen(s);
    memcpy(buf, s, (size_t)l);
    *n = *pos = l; show(buf, l);
}

int readline(char *buf, int max) {
    int n = 0, pos = 0, hi = hist_cnt;
    for (;;) {
        int c = console_getc_nb();
        if (c < 0) {
            thread_yield();
            if (!others_ready()) __asm__ volatile("sti; hlt");
            continue;
        }
        if (c == '\n') {
            show(buf + pos, n - pos);                /* kursor ke ujung dulu, lalu baris baru */
            kputc('\n'); buf[n] = 0;
            if (n > 0 && (hist_cnt == 0 || strcmp(hist[hist_cnt - 1], buf))) {
                if (hist_cnt == HIST_N) { memmove(hist[0], hist[1], sizeof(hist[0]) * (HIST_N - 1)); hist_cnt--; }
                strncpy(hist[hist_cnt++], buf, 127);
            }
            return n;
        }
        if (c == '\b') {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, (size_t)(n - pos));
                n--; pos--; back(1);
                show(buf + pos, n - pos); kputc(' ');
                back(n - pos + 1);
            }
        } else if (c == KEY_DEL) {
            if (pos < n) {
                memmove(buf + pos, buf + pos + 1, (size_t)(n - pos - 1));
                n--;
                show(buf + pos, n - pos); kputc(' ');
                back(n - pos + 1);
            }
        } else if (c == KEY_LEFT)  { if (pos > 0) { back(1); pos--; } }
        else if (c == KEY_RIGHT)   { if (pos < n) { kputc(buf[pos]); pos++; } }
        else if (c == KEY_HOME)    { back(pos); pos = 0; }
        else if (c == KEY_END)     { show(buf + pos, n - pos); pos = n; }
        else if (c == KEY_UP)      { if (hi > 0) { hi--; replace(buf, &n, &pos, hist[hi]); } }
        else if (c == KEY_DOWN)    { if (hi < hist_cnt) { hi++; replace(buf, &n, &pos, hi == hist_cnt ? "" : hist[hi]); } }
        else if (c >= 32 && c < 127 && n < max - 1) {
            memmove(buf + pos + 1, buf + pos, (size_t)(n - pos));
            buf[pos] = (char)c; n++;
            show(buf + pos, n - pos);                /* cetak karakter baru + sisa baris di kanannya */
            pos++;
            back(n - pos);                           /* kursor kembali tepat setelah karakter baru */
        }
    }
}