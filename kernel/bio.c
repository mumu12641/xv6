// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// refer to https://blog.miigon.net/posts/s081-lab8-locks/
extern uint ticks;
struct {
    struct spinlock eviction_lock;
    struct buf buf[NBUF];

    struct buf buf_hash[NBUCKET];
    struct spinlock buf_hash_locks[NBUCKET];
} bcache;

void binit(void) {
    // struct buf *b;

    initlock(&bcache.eviction_lock, "bcache_eviction");

    for (int i = 0; i < NBUCKET; i++) {
        initlock(&bcache.buf_hash_locks[i], "bcache_hash");
        bcache.buf_hash[i].next = 0;
    }

    // put all buffers to hash table[0]
    for (int i = 0; i < NBUF; i++) {
        struct buf *b = &bcache.buf[i];
        initsleeplock(&b->lock, "buffer");
        b->lastuse = 0;
        b->refcnt = 0;
        b->next = bcache.buf_hash[0].next;
        bcache.buf_hash[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
    struct buf *b;

    uint key = BUFMAP_HASH(blockno);
    acquire(&bcache.buf_hash_locks[key]);

    // Is the block already cached?
    for (b = bcache.buf_hash[key].next; b; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.buf_hash_locks[key]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    release(&bcache.buf_hash_locks[key]);

    acquire(&bcache.eviction_lock);
    for (b = bcache.buf_hash[key].next; b; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            acquire(&bcache.buf_hash_locks[key]);
            b->refcnt++;
            release(&bcache.buf_hash_locks[key]);

            release(&bcache.eviction_lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    //     if (b->dev == dev && b->blockno == blockno) {
    //         b->refcnt++;
    //         release(&bcache.lock);
    //         acquiresleep(&b->lock);
    //         return b;
    //     }
    // }

    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.

    // buf_find find the previous one that meets the requirements
    struct buf *buf_find = 0;
    uint flag = 0;
    uint before = -1;
    for (int i = 0; i < NBUCKET; i++) {
        if (i == key) continue;
        acquire(&bcache.buf_hash_locks[i]);

        // buf_find = bcache.buf_hash[i].next;
        for (b = &bcache.buf_hash[i]; b->next; b = b->next) {
            if (b->next->refcnt == 0 &&
                ((!buf_find) || b->next->lastuse < buf_find->next->lastuse)) {
                flag = 1;
                buf_find = b;
            }
        }
        if (flag == 0) {
            release(&bcache.buf_hash_locks[i]);
        } else {
            if (before != -1) release(&bcache.buf_hash_locks[before]);
            before = i;
        }
        flag = 0;
    }
    if (!buf_find) panic("bget: no buffers");
    
    b = buf_find->next;
    buf_find->next = b->next;
    release(&bcache.buf_hash_locks[before]);

    acquire(&bcache.buf_hash_locks[key]);
    b->next = bcache.buf_hash[key].next;
    bcache.buf_hash[key].next = b;

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.buf_hash_locks[key]);
    release(&bcache.eviction_lock);
    acquiresleep(&b->lock);
    return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock)) panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
    if (!holdingsleep(&b->lock)) panic("brelse");

    releasesleep(&b->lock);

    uint key = BUFMAP_HASH(b->blockno);
    acquire(&bcache.buf_hash_locks[key]);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        // b->next->prev = b->prev;
        // b->prev->next = b->next;
        // b->next = bcache.head.next;
        // b->prev = &bcache.head;
        // bcache.head.next->prev = b;
        // bcache.head.next = b;
        b->lastuse = ticks;
    }

    release(&bcache.buf_hash_locks[key]);
}

void bpin(struct buf *b) {
    uint key = BUFMAP_HASH(b->blockno);
    acquire(&bcache.buf_hash_locks[key]);
    b->refcnt++;
    release(&bcache.buf_hash_locks[key]);
}

void bunpin(struct buf *b) {
    uint key = BUFMAP_HASH(b->blockno);
    acquire(&bcache.buf_hash_locks[key]);
    b->refcnt--;
    release(&bcache.buf_hash_locks[key]);
}
