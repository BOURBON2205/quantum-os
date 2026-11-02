/* thread.c - TCB, status thread, scheduler Round Robin preemptive + context switch */
#include "kernel.h"

#define STACK_SIZE 8192

extern void switch_context(u32 *old_esp, u32 new_esp);

static thread_t table[MAX_THREADS];
thread_t *current;
int trace_on;
u32 ctx_switches;
static int next_tid = 1;
static int slice = QUANTUM;

const char *state_name(enum tstate s) {
    static const char *n[] = { "UNUSED", "NEW", "READY", "RUNNING", "WAITING", "TERMINATED" };
    return n[s];
}

thread_t *thread_table(int i) { return &table[i]; }

static void set_state(thread_t *t, enum tstate s) {
    if (trace_on && t->state != s && t != &table[1])
        kprintf("  [state] T%d %s: %s -> %s\n", t->tid, t->name, state_name(t->state), state_name(s));
    t->state = s;
}

static void idle_fn(void *a) { (void)a; for (;;) __asm__ volatile("sti; hlt"); }

static void thread_start(void) {            /* titik masuk pertama setiap thread baru */
    __asm__ volatile("sti");
    current->fn(current->arg);
    thread_exit();
}

void threads_init(void) {
    memset(table, 0, sizeof(table));
    /* slot 0 = thread utama (shell), memakai stack boot */
    table[0].tid = next_tid++; strncpy(table[0].name, "shell", 15); table[0].state = T_RUNNING;
    current = &table[0];
    thread_create("idle", idle_fn, 0);       /* slot 1 */
}

thread_t *thread_create(const char *name, void (*fn)(void *), void *arg) {
    u32 f = irq_save();
    thread_t *t = 0;
    for (int i = 1; i < MAX_THREADS; i++) {
        if (table[i].state == T_UNUSED || table[i].state == T_TERMINATED) {
            if (table[i].stack) { kfree(table[i].stack); table[i].stack = 0; }
            t = &table[i]; break;
        }
    }
    if (!t) { irq_restore(f); return 0; }
    u8 *stk = kmalloc(STACK_SIZE);
    if (!stk) { irq_restore(f); return 0; }
    memset(t, 0, sizeof(*t));
    t->tid = next_tid++; strncpy(t->name, name, 15);
    t->stack = stk; t->fn = fn; t->arg = arg;
    /* siapkan stack awal: pop edi,esi,ebx,ebp lalu ret -> thread_start */
    u32 *sp = (u32 *)(stk + STACK_SIZE);
    *--sp = 0;                                /* alamat balik palsu */
    *--sp = (u32)thread_start;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    t->esp = (u32)sp;
    t->state = T_NEW;
    if (trace_on) kprintf("  [state] T%d %s: (baru) -> NEW\n", t->tid, t->name);
    set_state(t, T_READY);
    irq_restore(f);
    return t;
}

int others_ready(void) {
    for (int i = 2; i < MAX_THREADS; i++)
        if (&table[i] != current && table[i].state == T_READY) return 1;
    return 0;
}

void schedule(void) {
    u32 f = irq_save();
    thread_t *prev = current, *next = 0;
    int pi = (int)(prev - table);
    for (int k = 1; k <= MAX_THREADS; k++) {          /* Round Robin, idle (slot 1) dilewati */
        int i = (pi + k) % MAX_THREADS;
        if (i != 1 && table[i].state == T_READY) { next = &table[i]; break; }
    }
    if (!next && prev->state != T_RUNNING && table[1].state == T_READY) next = &table[1];
    slice = QUANTUM;
    if (!next || next == prev) { irq_restore(f); return; }
    if (prev->state == T_RUNNING) set_state(prev, T_READY);
    set_state(next, T_RUNNING);
    current = next;
    ctx_switches++;
    switch_context(&prev->esp, next->esp);
    irq_restore(f);
}

void timer_tick(void) {
    ticks++;
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &table[i];
        if (t->state == T_WAITING && t->sleeping && (int)(ticks - t->wake_tick) >= 0) {
            t->sleeping = 0; set_state(t, T_READY);
        }
    }
    current->runtime++;
    if (--slice <= 0) schedule();
}

void thread_yield(void) { schedule(); }

void thread_exit(void) {
    irq_save();
    set_state(current, T_TERMINATED);
    schedule();
    for (;;) __asm__ volatile("hlt");
}

void thread_sleep(u32 t) {
    u32 f = irq_save();
    current->wake_tick = ticks + t; current->sleeping = 1;
    set_state(current, T_WAITING);
    schedule();
    irq_restore(f);
}

void thread_block(void) {           /* dipanggil dengan interrupt nonaktif */
    set_state(current, T_WAITING);
    schedule();
}

void thread_wake(thread_t *t) { set_state(t, T_READY); }

int thread_kill(int tid) {
    u32 f = irq_save(); int r = -1;
    for (int i = 2; i < MAX_THREADS; i++)
        if (table[i].tid == tid && &table[i] != current &&
            table[i].state != T_UNUSED && table[i].state != T_TERMINATED) {
            table[i].sleeping = 0; set_state(&table[i], T_TERMINATED); r = 0;
        }
    irq_restore(f);
    return r;
}

void threads_dump(void) {
    kprintf("TID  NAMA        STATE        CPU(tick)\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &table[i];
        if (t->state == T_UNUSED) continue;
        kprintf("%3d  %s", t->tid, t->name);
        for (int k = (int)strlen(t->name); k < 12; k++) kputc(' ');
        kprintf("%s", state_name(t->state));
        for (int k = (int)strlen(state_name(t->state)); k < 13; k++) kputc(' ');
        kprintf("%u\n", t->runtime);
    }
    kprintf("Context switch total: %u, quantum: %d tick (%d ms)\n", ctx_switches, QUANTUM, QUANTUM * 1000 / TICK_HZ);
}
