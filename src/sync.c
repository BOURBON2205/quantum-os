/* sync.c - mutex & semaphore (antrian tunggu FIFO), deteksi deadlock via wait-for graph */
#include "kernel.h"

static void wq_push(struct wq *q, thread_t *t) {
    t->wnext = 0;
    if (q->tail) q->tail->wnext = t; else q->head = t;
    q->tail = t;
}
static thread_t *wq_pop(struct wq *q) {
    thread_t *t = q->head;
    if (t) { q->head = t->wnext; if (!q->head) q->tail = 0; t->wnext = 0; }
    return t;
}

void mutex_init(mutex_t *m, const char *name) { m->locked = 0; m->owner = 0; m->q.head = m->q.tail = 0; m->name = name; }

void mutex_lock(mutex_t *m) {
    u32 f = irq_save();
    while (m->locked) {                  /* tunggu sampai dibangunkan, lalu coba ambil lagi */
        current->wait_mutex = m;
        wq_push(&m->q, current);
        thread_block();
        current->wait_mutex = 0;
    }
    m->locked = 1; m->owner = current;
    irq_restore(f);
}

void mutex_unlock(mutex_t *m) {
    u32 f = irq_save();
    m->locked = 0; m->owner = 0;
    thread_t *w = wq_pop(&m->q);
    if (w) thread_wake(w);
    irq_restore(f);
}

void sem_init(sem_t *s, int n) { s->count = n; s->q.head = s->q.tail = 0; }

void sem_wait(sem_t *s) {
    u32 f = irq_save();
    if (s->count > 0) s->count--;
    else { wq_push(&s->q, current); thread_block(); }
    irq_restore(f);
}

void sem_post(sem_t *s) {
    u32 f = irq_save();
    thread_t *w = wq_pop(&s->q);
    if (w) thread_wake(w); else s->count++;
    irq_restore(f);
}

/* Cari siklus: thread -> (menunggu mutex) -> pemilik mutex -> ... kembali ke thread awal */
int detect_deadlock(void) {
    int found = 0;
    for (int i = 2; i < MAX_THREADS; i++) {
        thread_t *s = thread_table(i);
        if (s->state != T_WAITING || !s->wait_mutex) continue;
        thread_t *c = s;
        for (int step = 0; step < MAX_THREADS && c && c->wait_mutex; step++) {
            c = c->wait_mutex->owner;
            if (c == s) {
                kprintf("DEADLOCK terdeteksi (circular wait): ");
                thread_t *p = s;
                do {
                    kprintf("T%d(%s) --minta %s--> ", p->tid, p->name, p->wait_mutex->name);
                    p = p->wait_mutex->owner;
                } while (p != s);
                kprintf("T%d(%s)\n", s->tid, s->name);
                found = 1; break;
            }
        }
        if (found) break;
    }
    return found;
}
