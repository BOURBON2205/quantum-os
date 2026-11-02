#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define OS_NAME "QuantumOS"
#define OS_VER  "1.0"

/* ---------- port I/O & kontrol interrupt ---------- */
static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8   inb(u16 p)        { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32  irq_save(void)    { u32 f; __asm__ volatile("pushf; pop %0; cli" : "=r"(f) :: "memory"); return f; }
static inline void irq_restore(u32 f){ __asm__ volatile("push %0; popf" :: "r"(f) : "memory", "cc"); }

/* ---------- string.c ---------- */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strncpy(char *d, const char *s, size_t n);
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
int    atoi(const char *s);

/* ---------- console.c ---------- */
void console_init(void);
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void console_clear(void);
int  console_getc_nb(void);          /* -1 jika tidak ada input */
int  readline(char *buf, int max);

/* ---------- interrupts.c ---------- */
void interrupts_init(void);
extern volatile u32 ticks;           /* 100 tick per detik */
#define TICK_HZ 100

/* ---------- mem.c ---------- */
void  mem_init(void);
void *kmalloc(u32 n);
void  kfree(void *p);
struct meminfo { u32 total, used, free, nblocks, nalloc, nfree, nfail; };
void  mem_info(struct meminfo *mi);

/* ---------- thread.c ---------- */
enum tstate { T_UNUSED, T_NEW, T_READY, T_RUNNING, T_WAITING, T_TERMINATED };
struct mutex;
typedef struct thread {
    int           tid;
    char          name[16];
    enum tstate   state;
    u32           esp;
    u8           *stack;
    void        (*fn)(void *);
    void         *arg;
    u32           wake_tick;
    int           sleeping;
    struct mutex *wait_mutex;
    struct thread *wnext;
    u32           runtime;
} thread_t;

#define MAX_THREADS 16
#define QUANTUM     3                /* tick per time-slice (30 ms) */

void      threads_init(void);
thread_t *thread_create(const char *name, void (*fn)(void *), void *arg);
void      thread_exit(void);
void      thread_yield(void);
void      thread_sleep(u32 t);
void      thread_block(void);
void      thread_wake(thread_t *t);
int       thread_kill(int tid);
void      schedule(void);
void      timer_tick(void);
void      threads_dump(void);
const char *state_name(enum tstate s);
thread_t *thread_table(int i);
extern thread_t *current;
extern int trace_on;
extern u32 ctx_switches;
int       others_ready(void);

/* ---------- sync.c ---------- */
struct wq { thread_t *head, *tail; };
typedef struct mutex { int locked; thread_t *owner; struct wq q; const char *name; } mutex_t;
typedef struct { int count; struct wq q; } sem_t;
void mutex_init(mutex_t *m, const char *name);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
void sem_init(sem_t *s, int n);
void sem_wait(sem_t *s);
void sem_post(sem_t *s);
int  detect_deadlock(void);

/* ---------- fs.c ---------- */
#define E_NOENT (-1)
#define E_EXIST (-2)
#define E_PERM  (-3)
#define E_NOMEM (-4)
#define E_FULL  (-5)
#define P_R 4
#define P_W 2
void        fs_init(void);
int         fs_create(const char *name);
int         fs_write(const char *name, const char *txt, int append);
int         fs_read(const char *name, const char **data, u32 *size);
int         fs_delete(const char *name);
int         fs_chmod(const char *name, int owner, int other);
void        fs_list(void);
const char *fs_err(int e);

/* ---------- user / hak akses ---------- */
extern int cur_uid;                  /* 0 = root, 1 = guest */
const char *uid_name(int uid);
int  user_switch(const char *name, const char *pass);

/* ---------- demo.c & shell.c ---------- */
void demo_threads(void);
void demo_race(void);
void demo_deadlock(int fix);
void demo_memtest(void);
void shell_run(void);
void banner(void);
#endif
