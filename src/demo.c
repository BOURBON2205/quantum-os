/* demo.c - skenario pengujian: thread, race condition, deadlock, memori */
#include "kernel.h"

static sem_t done;

static void busy_ticks(u32 n) { u32 end = ticks + n; while ((int)(ticks - end) < 0) ; }

/* ---------- 1. thread & perubahan status ---------- */
static void worker_a(void *a) {
    (void)a;
    for (int i = 1; i <= 3; i++) { kprintf("  [worker-A] kerja CPU, iterasi %d\n", i); busy_ticks(4); }
    sem_post(&done);
}
static void worker_b(void *a) {
    (void)a;
    for (int i = 1; i <= 3; i++) { kprintf("  [worker-B] tidur 50 ms, iterasi %d\n", i); thread_sleep(5); }
    sem_post(&done);
}

void demo_threads(void) {
    int old = trace_on; trace_on = 1;
    sem_init(&done, 0);
    kprintf("Membuat 2 kernel thread (tugas berbeda)...\n");
    thread_create("worker-A", worker_a, 0);
    thread_create("worker-B", worker_b, 0);
    sem_wait(&done); sem_wait(&done);        /* shell sendiri ikut WAITING di sini */
    trace_on = old;
    kprintf("Selesai. Status akhir:\n");
    threads_dump();
}

/* ---------- 2. race condition vs mutex ---------- */
#define RACE_N 1000000
static volatile int shared;
static int use_lock;
static mutex_t cmu;

static void race_worker(void *a) {
    (void)a;
    for (int i = 0; i < RACE_N; i++) {
        if (use_lock) mutex_lock(&cmu);
        int tmp = shared;                              /* read  */
        for (volatile int d = 0; d < 20; d++) ;         /* jendela kritis (simulasi komputasi) */
        shared = tmp + 1;                              /* write */
        if (use_lock) mutex_unlock(&cmu);
    }
    sem_post(&done);
}

static void race_once(int lock) {
    use_lock = lock; shared = 0;
    mutex_init(&cmu, "counter-lock"); sem_init(&done, 0);
    u32 c0 = ctx_switches;
    thread_create("race-1", race_worker, 0);
    thread_create("race-2", race_worker, 0);
    sem_wait(&done); sem_wait(&done);
    kprintf("  %s: hasil=%d, seharusnya=%d, context switch=%u -> %s\n",
            lock ? "DENGAN mutex " : "TANPA sinkronisasi", shared, 2 * RACE_N, ctx_switches - c0,
            shared == 2 * RACE_N ? "KONSISTEN" : "RACE CONDITION (update hilang)");
}

void demo_race(void) {
    kprintf("2 thread masing-masing menambah shared counter %d kali.\n", RACE_N);
    race_once(0);
    race_once(1);
}

/* ---------- 3. deadlock ---------- */
static mutex_t ma, mb;
static int ordered;

static void dl_1(void *a) {
    (void)a;
    mutex_lock(&ma); kprintf("  [dl-1] memegang A\n");
    thread_sleep(10);
    kprintf("  [dl-1] minta B...\n");
    mutex_lock(&mb); kprintf("  [dl-1] memegang A dan B, selesai\n");
    mutex_unlock(&mb); mutex_unlock(&ma);
    sem_post(&done);
}
static void dl_2(void *a) {
    (void)a;
    mutex_t *first = ordered ? &ma : &mb, *second = ordered ? &mb : &ma;
    mutex_lock(first); kprintf("  [dl-2] memegang %s\n", first->name);
    thread_sleep(10);
    kprintf("  [dl-2] minta %s...\n", second->name);
    mutex_lock(second); kprintf("  [dl-2] memegang A dan B, selesai\n");
    mutex_unlock(second); mutex_unlock(first);
    sem_post(&done);
}

void demo_deadlock(int fix) {
    for (int i = 2; i < MAX_THREADS; i++) {              /* bersihkan thread macet dari demo sebelumnya */
        thread_t *t = thread_table(i);
        if (t->state == T_WAITING && !strncmp(t->name, "dl-", 3)) thread_kill(t->tid);
    }
    ordered = fix;
    mutex_init(&ma, "mutex-A"); mutex_init(&mb, "mutex-B"); sem_init(&done, 0);
    kprintf(fix ? "Skenario SOLUSI: kedua thread mengambil mutex dengan urutan sama (A lalu B).\n"
                : "Skenario DEADLOCK: dl-1 ambil A->B, dl-2 ambil B->A.\n");
    thread_create("dl-1", dl_1, 0);
    thread_create("dl-2", dl_2, 0);
    if (fix) {
        sem_wait(&done); sem_wait(&done);
        kprintf("Kedua thread selesai tanpa deadlock (circular wait dicegah).\n");
    } else {
        thread_sleep(60);                                 /* beri waktu thread saling mengunci */
        if (!detect_deadlock()) kprintf("Tidak ada deadlock.\n");
        kprintf("Cek 'ps': kedua thread WAITING selamanya. Bersihkan dengan 'kill <tid>'.\n");
    }
}

/* ---------- 4. memori ---------- */
static void print_mem(void) {
    struct meminfo m; mem_info(&m);
    kprintf("  total=%u  terpakai=%u  tersedia=%u  blok=%u  (alloc=%u free=%u gagal=%u)\n",
            m.total, m.used, m.free, m.nblocks, m.nalloc, m.nfree, m.nfail);
}

void demo_memtest(void) {
    void *p[64]; int n = 0;
    kprintf("Kondisi awal:\n"); print_mem();
    while (n < 64 && (p[n] = kmalloc(16 * 1024)) != 0) n++;
    kprintf("Alokasi 16 KB berulang sampai habis: %d blok berhasil, berikutnya NULL (OOM ditangani, kernel tetap hidup)\n", n);
    print_mem();
    for (int i = 0; i < n; i++) kfree(p[i]);
    kprintf("Semua blok dilepas:\n"); print_mem();
}
