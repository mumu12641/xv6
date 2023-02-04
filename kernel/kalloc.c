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
} kmem[NCPU];

char *kmem_lock_names[] = {
    "kmem_cpu_0", "kmem_cpu_1", "kmem_cpu_2", "kmem_cpu_3",
    "kmem_cpu_4", "kmem_cpu_5", "kmem_cpu_6", "kmem_cpu_7",
};

void kinit() {
    // initlock(&kmem.lock, "kmem");
    for (int i = 0; i < NCPU; i++) {
        initlock(&kmem[i].lock, kmem_lock_names[i]);
    }
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
    struct run *r;

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    push_off();

    int cpu = cpuid();
    acquire(&kmem[cpu].lock);

    r->next = kmem[cpu].freelist;
    kmem[cpu].freelist = r;

    release(&kmem[cpu].lock);

    pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
    struct run *r;

    push_off();
    int cpu = cpuid();
    acquire(&kmem[cpu].lock);
    r = kmem[cpu].freelist;
    if (!r) {
        int steal_pages = 32;
        for (int i = 0; i < NCPU; i++) {
            if (i == cpu) continue;
            acquire(&kmem[i].lock);
            struct run *st = kmem[i].freelist;
            while (st && steal_pages) {
                struct run *temp;
                temp = st->next;
                st->next = kmem[cpu].freelist;
                kmem[cpu].freelist = st;
                kmem[i].freelist = temp;
                st = kmem[i].freelist;
                steal_pages--;
            }
            release(&kmem[i].lock);
            if (steal_pages == 0 || i == NCPU) {
                break;
            }
        }
    }
    r = kmem[cpu].freelist;
    if (r) {
        kmem[cpu].freelist = r->next;
    }
    release(&kmem[cpu].lock);
    pop_off();

    if (r) memset((char *)r, 5, PGSIZE);  // fill with junk
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