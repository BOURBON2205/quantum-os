/* shell.c - CLI QuantumOS */
#include "kernel.h"

static void *h_ptr[16]; static u32 h_sz[16];

static void draw_quantumos(void) {
    static const char font[9][7][8] = {
        { ".#####.", "##...##", "##...##", "##...##", "##.#.##", "##..###", ".######" }, /* Q */
        { "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####." }, /* U */
        { "..###..", ".##.##.", "##...##", "##...##", "#######", "##...##", "##...##" }, /* A */
        { "##...##", "###..##", "####.##", "##.####", "##..###", "##...##", "##...##" }, /* N */
        { "#######", "...#...", "...#...", "...#...", "...#...", "...#...", "...#..." }, /* T */
        { "##...##", "###.###", "#######", "##.#.##", "##...##", "##...##", "##...##" }, /* M */
        { ".#####.", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####." }, /* O */
        { ".######", "##.....", "##.....", ".#####.", ".....##", ".....##", "######." }, /* S */
    };
    static const u8 letters[9] = { 0, 1, 2, 3, 4, 1, 5, 6, 7 };
    for (int row = 0; row < 7; row++) {
        kputs("    ");
        for (int letter = 0; letter < 9; letter++) {
            for (int col = 0; col < 7; col++)
                kputc(font[letters[letter]][row][col] == '#' ? '#' : ' ');
            if (letter < 8) kputc(' ');
        }
        kputc('\n');
    }
}

void banner(void) {                                /* kalkulator basis QuantumOS */
    console_color(11);
    draw_quantumos();
    console_color(9);
    kputs(
"  .--------.        .------( |0> )------.\n"
"--| Q-CORE |--o----(       Q       )----o---[ i386 ]\n"
"  '--------'        '------( |1> )------'\n");
    console_color(11); kputs("NUMBER BASE CALCULATOR\n");
    console_color(7);  kputs("BIN <-> OCT <-> DEC <-> HEX\n");
    console_color(15); kprintf("%s v%s | i386 protected mode\n", OS_NAME, OS_VER);
    console_color(14); kputs("Ketik 'convert' lalu klik basis asal/tujuan | 'help umum'\n");
    console_color(8);  kputs("-------------------------------------------------------------------------------\n");
    console_color(15);
    console_set_header_end();
}
static void row(const char *k, const char *v) {      /* perintah (kuning) + penjelasan (putih) */
    console_color(14); kprintf("%s", k);
    for (int i = (int)strlen(k); i < 20; i++) kputc(' ');
    console_color(15); kprintf("%s\n", v);
}
static void title(const char *t) { console_color(11); kprintf("%s\n", t); console_color(15); }
static void note(const char *t)  { console_color(7);  kprintf("%s\n", t); console_color(15); }

static void help(const char *t) {
    if (t && !strcmp(t, "umum")) {
        title("[UMUM] info dan kontrol dasar");
        row("about",          "tampilkan info versi OS");
        row("clear",          "bersihkan layar (logo tetap di atas)");
        row("echo <teks>",    "cetak teks. Contoh: echo halo");
        row("uptime",         "sudah berapa lama OS menyala");
        row("convert", "pilih angka, asal, dan tujuan dengan klik");
        note("Contoh langsung: convert 2A hex bin");
        row("reboot / halt",  "restart OS / hentikan OS");
    } else if (t && !strcmp(t, "thread")) {
        title("[THREAD] alur kerja kecil di dalam kernel dan penjadwalannya");
        row("ps",             "daftar thread + status (READY, RUNNING, WAITING, ...)");
        row("threads",        "DEMO: jalankan 2 thread, tampilkan perubahan statusnya");
        row("sched",          "info scheduler: Round Robin, quantum, jumlah ganti thread");
        row("trace on|off",   "tampilkan/sembunyikan log perubahan status thread");
        row("kill <tid>",     "hentikan thread. Nomor tid lihat di 'ps'");
        note("Mulai dari: threads  lalu  ps");
    } else if (t && !strcmp(t, "sinkron")) {
        title("[SINKRON] dua thread berbagi data: race condition dan deadlock");
        row("race",           "DEMO race condition: tanpa lock salah, dengan mutex benar");
        note("                    (butuh +-20 detik, tunggu sampai kedua hasil muncul)");
        row("deadlock",       "DEMO: 2 thread saling menunggu, lalu terdeteksi OS");
        row("deadlock fix",   "DEMO solusi: urutan ambil mutex disamakan -> tidak macet");
        note("Setelah 'deadlock': ketik ps, lalu kill <tid> untuk membersihkan");
    } else if (t && !strcmp(t, "memori")) {
        title("[MEMORI] alokator heap 512 KB");
        row("meminfo",        "total, terpakai, dan tersedia");
        row("alloc <byte>",   "minta memori, dapat nomor handle. Contoh: alloc 1000");
        row("free <handle>",  "lepas memori. Contoh: free 0");
        row("memtest",        "DEMO: habiskan memori sampai Out of Memory (aman)");
    } else if (t && !strcmp(t, "berkas")) {
        title("[BERKAS] sistem berkas di RAM (hilang saat OS mati)");
        row("ls",                  "daftar berkas, pemilik, dan izin");
        row("touch <f>",           "buat berkas kosong");
        row("write <f> <teks>",    "isi berkas (menimpa isi lama)");
        row("append <f> <teks>",   "tambah teks di akhir berkas");
        row("cat <f>",             "tampilkan isi berkas");
        row("rm <f>",              "hapus berkas");
        note("Contoh: touch a.txt  ->  write a.txt halo  ->  cat a.txt");
    } else if (t && !strcmp(t, "akses")) {
        title("[AKSES] user dan izin berkas");
        row("whoami",              "user saat ini (awal: guest)");
        row("su <user> [pass]",    "ganti user: root (password: toor) atau guest");
        row("chmod <OW> <f>",      "atur izin: O=pemilik, W=lainnya (dua digit)");
        note("                    angka: 4=baca  2=tulis  6=baca+tulis  0=tidak ada");
        note("Coba: cat secret.txt (ditolak) -> su root toor -> cat secret.txt");
    } else {
        if (t) { console_color(12); kprintf("Kategori '%s' tidak ada.\n", t); console_color(15); }
        title("QuantumOS - ketik 'help <kategori>' untuk penjelasan + contoh");
        row("help umum",    "info OS, bersihkan layar, uptime, restart/matikan");
        row("convert", "kalkulator klik BIN/OCT/DEC/HEX");
        row("help thread",  "lihat thread dan statusnya, info scheduler");
        row("help sinkron", "demo race condition dan deadlock (mutex)");
        row("help memori",  "lihat, minta, lepas memori; uji Out of Memory");
        row("help berkas",  "buat, baca, ubah, hapus berkas");
        row("help akses",   "ganti user dan atur izin berkas");
        note("Mulai konversi: ketik convert, lalu pilih basis asal dan tujuan");
        note("Contoh langsung: convert 2A hex bin");
        note("Demo lain: threads | race | deadlock | memtest");
        note("Edit baris: kiri/kanan geser kursor, atas/bawah riwayat, Home End Del");
    }
}

static int split(char *line, char **argv, int max) {
    int n = 0;
    while (*line && n < max) {
        while (*line == ' ') *line++ = 0;
        if (!*line) break;
        argv[n++] = line;
        while (*line && *line != ' ') line++;
    }
    return n;
}

static const char *rest(const char *line_orig, int skip) {      /* ambil sisa baris setelah 'skip' kata */
    const char *p = line_orig;
    for (int i = 0; i < skip; i++) { while (*p == ' ') p++; while (*p && *p != ' ') p++; }
    while (*p == ' ') p++;
    return p;
}

static void reboot(void) { outb(0x64, 0xFE); for (;;) __asm__ volatile("hlt"); }

static void report(int r) { if (r < 0) kprintf("Error: %s\n", fs_err(r)); }

static int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int number_base(const char *s) {
    if (!strcmp(s, "bin") || !strcmp(s, "binary") || !strcmp(s, "biner") || !strcmp(s, "2")) return 2;
    if (!strcmp(s, "oct") || !strcmp(s, "octal") || !strcmp(s, "oktal") || !strcmp(s, "8")) return 8;
    if (!strcmp(s, "dec") || !strcmp(s, "decimal") || !strcmp(s, "desimal") || !strcmp(s, "10")) return 10;
    if (!strcmp(s, "hex") || !strcmp(s, "hexadecimal") || !strcmp(s, "heksadesimal") || !strcmp(s, "16")) return 16;
    return 0;
}

static int parse_number(const char *s, int requested_base, u32 *value, int *negative) {
    int sign = 0;
    if (*s == '-' || *s == '+') { sign = (*s == '-'); s++; }
    if (!*s) return 0;

    int prefix_base = 0;
    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) prefix_base = 2;
    else if (s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) prefix_base = 8;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) prefix_base = 16;
    if (prefix_base) {
        if (requested_base && requested_base != prefix_base) return 0;
        requested_base = prefix_base;
        s += 2;
    }
    int base = requested_base ? requested_base : 10;
    if (!*s) return 0;

    u32 n = 0;
    for (; *s; s++) {
        int d = digit_value(*s);
        if (d < 0 || d >= base || n > (0xFFFFFFFFU - (u32)d) / (u32)base) return 0;
        n = n * (u32)base + (u32)d;
    }
    if (sign && n > 0x80000000U) return 0;
    *value = n;
    *negative = sign;
    return 1;
}

static const char *base_label(int base) {
    switch (base) {
    case 2: return "BIN";
    case 8: return "OCT";
    case 10: return "DEC";
    default: return "HEX";
    }
}

static void format_number(u32 value, int negative, int base, char *out) {
    static const char digits[] = "0123456789ABCDEF";
    char reverse[32];
    int n = 0, pos = 0;
    do { reverse[n++] = digits[value % (u32)base]; value /= (u32)base; } while (value);
    if (negative) out[pos++] = '-';
    while (n) out[pos++] = reverse[--n];
    out[pos] = 0;
}

static void show_all_bases(u32 value, int negative) {
    char bin[35], oct[35], dec[35], hex[35];
    format_number(value, negative, 2, bin);
    format_number(value, negative, 8, oct);
    format_number(value, negative, 10, dec);
    format_number(value, negative, 16, hex);
    kprintf("BIN: %s\nOCT: %s\nDEC: %s\nHEX: %s\n", bin, oct, dec, hex);
}

static void show_conversion(const char *number, int source, int target) {
    u32 value; int negative;
    if (!parse_number(number, source, &value, &negative)) {
        kputs("angka tidak valid atau melebihi rentang 32-bit\n");
        return;
    }
    if (target) {
        char result[35];
        format_number(value, negative, target, result);
        console_color(11); kprintf("%s: ", base_label(target));
        console_color(15); kprintf("%s\n", result);
    } else show_all_bases(value, negative);
}

static void ask_conversion_field(const char *prompt, char *value, int max) {
    console_color(11); kputs(prompt);
    console_color(15); readline(value, max);
}

static void draw_base_buttons(const char *label, int *row, int *start) {
    console_color(11); kprintf("%s: ", label);
    console_get_cursor(start, row);
    console_color(14); kputs("[ BIN ]  [ OCT ]  [ DEC ]  [ HEX ]\n");
    console_color(15);
}

static int base_from_button(int x, int y, int row, int start) {
    static const int values[4] = { 2, 8, 10, 16 };
    if (y != row) return 0;
    for (int i = 0; i < 4; i++)
        if (x >= start + i * 9 && x < start + i * 9 + 7) return values[i];
    return 0;
}

static int wait_for_base_button(int row, int start) {
    static const int values[4] = { 2, 8, 10, 16 };
    for (;;) {
        int x, y;
        if (console_get_mouse_click(&x, &y)) {
            int base = base_from_button(x, y, row, start);
            if (base) return base;
        }
        int key = console_getc_nb();
        if (key >= '1' && key <= '4') {
            kprintf("%c\n", key);
            return values[key - '1'];
        }
        if (key == 27) return 0;
        thread_yield();
        if (!others_ready()) __asm__ volatile("sti; hlt");
    }
}

static void convert_number(int ac, char **av) {
    char number[64];
    int source = 0, target = 0;
    const char *input;

    if (ac == 1) {
        ask_conversion_field("Angka: ", number, sizeof(number));
        if (!number[0]) { kputs("konversi dibatalkan\n"); return; }

        console_color(7);
        kputs("Klik tombol di jendela QEMU; tekan 1-4 sebagai alternatif.\n");
        int source_row, source_x;
        draw_base_buttons("DARI", &source_row, &source_x);
        source = wait_for_base_button(source_row, source_x);
        if (!source) { kputs("konversi dibatalkan\n"); return; }

        int target_row, target_x;
        draw_base_buttons("KE", &target_row, &target_x);
        target = wait_for_base_button(target_row, target_x);
        if (!target) { kputs("konversi dibatalkan\n"); return; }
        input = number;
    } else if (ac >= 2 && ac <= 4) {
        input = av[1];
        if (ac >= 3) source = number_base(av[2]);
        if (ac == 4) target = number_base(av[3]);
        if ((ac >= 3 && !source) || (ac == 4 && !target)) {
            kputs("basis tidak dikenal; gunakan bin, oct, dec, atau hex\n");
            return;
        }
    } else {
        kputs("pakai: convert [angka [asal [tujuan]]]\n");
        return;
    }

    show_conversion(input, source, target);
}

void shell_run(void) {
    char line[128], orig[128]; char *av[8];
    for (;;) {
        console_color(cur_uid == 0 ? 12 : 10); kprintf("%s@quantum", uid_name(cur_uid));
        console_color(14); kprintf("> "); console_color(15);
        readline(line, sizeof(line));
        memcpy(orig, line, sizeof(line));
        int ac = split(line, av, 8);
        if (!ac) continue;
        const char *c = av[0];
      if      (!strcmp(c, "help"))   help(ac > 1 ? av[1] : 0);
        else if (!strcmp(c, "convert")) convert_number(ac, av);
        else if (!strcmp(c, "about"))  kprintf("%s v%s | i386 protected mode | C + GNU as | preemptive RR | RAMFS\n", OS_NAME, OS_VER);
        else if (!strcmp(c, "clear"))  console_clear();
        else if (!strcmp(c, "echo"))   kprintf("%s\n", rest(orig, 1));
        else if (!strcmp(c, "uptime")) kprintf("Uptime: %u.%02u detik (%u tick)\n", ticks / TICK_HZ, ticks % TICK_HZ, ticks);
        else if (!strcmp(c, "reboot")) reboot();
        else if (!strcmp(c, "halt"))   { kprintf("Sistem berhenti.\n"); for (;;) __asm__ volatile("cli; hlt"); }
        else if (!strcmp(c, "ps"))     threads_dump();
        else if (!strcmp(c, "threads")) demo_threads();
        else if (!strcmp(c, "sched"))  kprintf("Scheduler: Round Robin preemptive, quantum %d tick = %d ms, timer PIT %d Hz, context switch: %u\n", QUANTUM, QUANTUM * 1000 / TICK_HZ, TICK_HZ, ctx_switches);
        else if (!strcmp(c, "trace"))  { if (ac > 1) trace_on = !strcmp(av[1], "on"); kprintf("trace perubahan status thread: %s\n", trace_on ? "ON" : "OFF"); }
        else if (!strcmp(c, "kill"))   { if (ac < 2) kprintf("pakai: kill <tid>\n"); else kprintf(thread_kill(atoi(av[1])) ? "tid tidak valid\n" : "thread dihentikan\n"); }
        else if (!strcmp(c, "race"))   demo_race();
        else if (!strcmp(c, "deadlock")) demo_deadlock(ac > 1 && !strcmp(av[1], "fix"));
        else if (!strcmp(c, "meminfo")) {
            struct meminfo m; mem_info(&m);
            kprintf("Heap total: %u B | terpakai: %u B | tersedia: %u B | blok: %u | alloc: %u | free: %u | gagal(OOM): %u\n", m.total, m.used, m.free, m.nblocks, m.nalloc, m.nfree, m.nfail);
        }
        else if (!strcmp(c, "alloc")) {
            if (ac < 2) { kprintf("pakai: alloc <byte>\n"); continue; }
            u32 n = (u32)atoi(av[1]); int id = -1;
            for (int i = 0; i < 16; i++) if (!h_ptr[i]) { id = i; break; }
            void *p = id >= 0 ? kmalloc(n) : 0;
            if (!p) kprintf("Gagal: %s\n", id < 0 ? "tabel handle penuh" : "Out of Memory (kernel tetap aman)");
            else { h_ptr[id] = p; h_sz[id] = n; kprintf("Handle %d: %u byte @ %p\n", id, n, p); }
        }
        else if (!strcmp(c, "free")) {
            int id = ac > 1 ? atoi(av[1]) : -1;
            if (id < 0 || id >= 16 || !h_ptr[id]) kprintf("handle tidak valid\n");
            else { kfree(h_ptr[id]); h_ptr[id] = 0; kprintf("Handle %d dilepas (%u byte)\n", id, h_sz[id]); }
        }
        else if (!strcmp(c, "memtest")) demo_memtest();
        else if (!strcmp(c, "ls"))     fs_list();
        else if (!strcmp(c, "touch"))  { if (ac < 2) kprintf("pakai: touch <file>\n"); else report(fs_create(av[1])); }
        else if (!strcmp(c, "write") || !strcmp(c, "append")) {
            if (ac < 3) { kprintf("pakai: %s <file> <teks>\n", c); continue; }
            int r = fs_write(av[1], rest(orig, 2), c[0] == 'a');
            report(r); if (!r) kprintf("OK\n");
        }
        else if (!strcmp(c, "cat")) {
            if (ac < 2) { kprintf("pakai: cat <file>\n"); continue; }
            const char *d; u32 sz; int r = fs_read(av[1], &d, &sz);
            if (r < 0) report(r); else { kputs(d); if (!sz || d[sz - 1] != '\n') kputc('\n'); }
        }
        else if (!strcmp(c, "rm"))     { if (ac < 2) kprintf("pakai: rm <file>\n"); else { int r = fs_delete(av[1]); report(r); if (!r) kprintf("OK\n"); } }
        else if (!strcmp(c, "whoami")) kprintf("%s (uid %d)\n", uid_name(cur_uid), cur_uid);
        else if (!strcmp(c, "su"))     {
            if (ac < 2) { kprintf("pakai: su <root|guest> [password]\n"); continue; }
            int r = user_switch(av[1], ac > 2 ? av[2] : 0);
            if (r < 0) kprintf("su: gagal (%s)\n", r == E_PERM ? "password salah" : "user tidak ada"); else kprintf("sekarang user: %s\n", uid_name(cur_uid));
        }
        else if (!strcmp(c, "chmod")) {
            if (ac < 3 || strlen(av[1]) != 2) { kprintf("pakai: chmod <OW> <file>  (mis. chmod 64 a.txt)\n"); continue; }
            int r = fs_chmod(av[2], av[1][0] - '0', av[1][1] - '0'); report(r); if (!r) kprintf("OK\n");
        }
        else kprintf("Perintah '%s' tidak dikenal. Ketik 'help'.\n", c);
    }
}
