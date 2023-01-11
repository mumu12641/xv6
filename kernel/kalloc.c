// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PA2PGREF_ID(p) ((((uint64)p) - KERNBASE) / PGSIZE)
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)

void freerange(void *pa_start, void *pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;


struct {
    struct spinlock lock;
    int cnt[PGREF_MAX_ENTRIES];
} ref;

void kinit() {
    initlock(&kmem.lock, "kmem");
    initlock(&ref.lock, "pgref");
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
        kfree(p);
    }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
    struct run *r;

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    acquire(&ref.lock);
    if (--ref.cnt[PA2PGREF_ID(pa)] <= 0) {
        // Fill with junk to catch dangling refs.
        // pa will be memset multiple times if race-condition occurred.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    }
    release(&ref.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r) kmem.freelist = r->next;
    release(&kmem.lock);

    if (r) {
        memset((char *)r, 5, PGSIZE);
        ref.cnt[PA2PGREF_ID(r)] = 1;
    }

    return (void *)r;
}

void *kcowcopy(void *pa) {
    acquire(&ref.lock);
    if (ref.cnt[PA2PGREF_ID(pa)] <= 1) {
        release(&ref.lock);
        return pa;
    }

    uint64 newpa = (uint64)kalloc();
    if (newpa == 0) {
        release(&ref.lock);
        return 0;
    }
    memmove((void *)newpa, (void *)pa, PGSIZE);

    ref.cnt[PA2PGREF_ID(pa)]--;

    release(&ref.lock);
    return (void *)newpa;
}

void addref(void *pa) {
    acquire(&ref.lock);
    ref.cnt[PA2PGREF_ID(pa)]++;
    release(&ref.lock);
}
uint64 get_free_memory(void) {
    // 锁
    acquire(&kmem.lock);

    uint64 mem_bytes = 0;
    struct run *r = kmem.freelist;
    while (r) {
        mem_bytes += PGSIZE;
        r = r->next;
    }

    release(&kmem.lock);
    return mem_bytes;
}