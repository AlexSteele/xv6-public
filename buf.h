
// An in-memory cache of a sequence of bytes from a file.
// Caches up to a page of bytes.
struct page {
  struct sleeplock lock;
  int flags;
  uint refcnt;
  uint nblocks; // Number of valid blocks.
  uint blocknos[8];

  // If this page is part of a memory-mapped
  // file, pointer to the next page in the mapping
  struct page *mapnext;
  uchar *data;
};

struct buf {
  int flags;
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  struct buf *qnext; // disk queue
//  uchar data[BSIZE];
  uchar *data;
};
#define B_VALID 0x2  // buffer or page has been read from disk
#define B_DIRTY 0x4  // buffer or page needs to be written to disk
#define B_MAPPED 0x8 // page is memory-mapped and shouldn't be touched
