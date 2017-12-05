
// An in-memory cache for of a sequence of PGSIZE bytes from a file.
// Valid if nblocks > 0.
struct page {
  struct sleeplock lock;
  uint refcnt;
  uint nblocks; // Number of disk blocks pinned to this page
  uint blocknos[8];
  int flags[8]; // Flags for each block

  // If this page is part of a memory-mapped
  // file, pointer to the next page in the mapping
  struct page *mapnext;

  // Page cache LRU list
  struct page *next;
  struct page *prev;
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
