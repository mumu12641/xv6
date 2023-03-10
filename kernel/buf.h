struct buf {
    int valid;  // has data been read from disk?
    int disk;   // does disk "own" buf?
    uint dev;
    uint blockno;
    struct sleeplock lock;
    uint refcnt;
    uint lastuse;
    struct buf *prev;  // LRU cache list
    struct buf *next;
    uchar data[BSIZE];
};

#define NBUCKET 13
#define BUFMAP_HASH(blockno) (blockno % NBUCKET)
