/* shell.c - CLI QuantumOS */
#include "kernel.h"

static void *h_ptr[16]; static u32 h_sz[16];

void banner(void) {
    kputs("\n"
"  ___  _   _   _    _   _ _____ _   _ __  __    ___  ____\n"
" / _ \\| | | | / \\  | \\ | |_   _| | | |  \\/  |  / _ \\/ ___|\n"
"| | | | | | |/ _ \\ |  \\| | | | | | | | |\\/| | | | | \\___ \\\n"
"| |_| | |_| / ___ \\| |\\  | | | | |_| | |  | | | |_| |___) |\n"
" \\__\\_\\\\___/_/   \\_\\_| \\_| |_|  \\___/|_|  |_|  \\___/|____/\n");
    kprintf("\n  %s v%s - Mini OS Tugas Besar Sistem Operasi (ITK)\n", OS_NAME, OS_VER);
    kprintf("  Ketik 'help' untuk daftar perintah.\n\n");
}

static void help(void) {
    kprintf(
"Umum     : help  about  clear  echo <teks>  uptime  reboot  halt\n"
"Thread   : ps  threads  sched  trace on|off  kill <tid>\n"
"Sinkron  : race  deadlock  deadlock fix\n"
"Memori   : meminfo  alloc <byte>  free <id>  memtest\n"
"Berkas   : ls  touch <f>  write <f> <teks>  append <f> <teks>  cat <f>  rm <f>\n"
"Akses    : whoami  su <root|guest> [password]  chmod <OW> <f>   (OW = dua digit oktal, mis. 64)\n");
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

void shell_run(void) {
    char line[128], orig[128]; char *av[8];
    for (;;) {
        kprintf("%s@quantum> ", uid_name(cur_uid));
        readline(line, sizeof(line));
        memcpy(orig, line, sizeof(line));
        int ac = split(line, av, 8);
        if (!ac) continue;
        const char *c = av[0];
        if      (!strcmp(c, "help"))   help();
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
