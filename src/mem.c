/* mem.c - alokator heap: linked list blok, first-fit, split & coalesce */
#include "kernel.h"

#define HEAP_SIZE (512 * 1024)
#define MAGIC 0xB0B0CAFE

struct blk { u32 size; u32 free; struct blk *next; u32 magic; };   /* 16 byte */

static u8 heap[HEAP_SIZE] __attribute__((aligned(16)));
static struct blk *head;
static u32 n_alloc, n_free, n_fail;

void mem_init(void) {
    head = (struct blk *)heap;
    head->size = HEAP_SIZE - sizeof(struct blk);
    head->free = 1; head->next = 0; head->magic = MAGIC;
}

void *kmalloc(u32 n) {
    if (n == 0) return 0;
    n = (n + 15) & ~15u;
    u32 f = irq_save();
    for (struct blk *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof(struct blk) + 16) {          /* split */
            struct blk *r = (struct blk *)((u8 *)(b + 1) + n);
            r->size = b->size - n - sizeof(struct blk);
            r->free = 1; r->next = b->next; r->magic = MAGIC;
            b->size = n; b->next = r;
        }
        b->free = 0; n_alloc++;
        irq_restore(f);
        return b + 1;
    }
    n_fail++;                                                   /* Out of Memory: aman, kembalikan NULL */
    irq_restore(f);
    return 0;
}

void kfree(void *p) {
    if (!p) return;
    u32 f = irq_save();
    struct blk *b = (struct blk *)p - 1;
    if (b->magic != MAGIC || b->free) { irq_restore(f); kprintf("kfree: pointer tidak valid / double free\n"); return; }
    b->free = 1; n_free++;
    for (struct blk *c = head; c && c->next; ) {                /* coalesce blok bebas bersebelahan */
        if (c->free && c->next->free) { c->size += sizeof(struct blk) + c->next->size; c->next = c->next->next; }
        else c = c->next;
    }
    irq_restore(f);
}

void mem_info(struct meminfo *mi) {
    u32 f = irq_save();
    memset(mi, 0, sizeof(*mi));
    mi->total = HEAP_SIZE;
    for (struct blk *b = head; b; b = b->next) {
        mi->nblocks++;
        if (b->free) mi->free += b->size;
    }
    mi->used = mi->total - mi->free;
    mi->nalloc = n_alloc; mi->nfree = n_free; mi->nfail = n_fail;
    irq_restore(f);
}
