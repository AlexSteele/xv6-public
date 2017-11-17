
struct page {
  struct sleeplock lock;
  int flags;
  uint refcnt;
  uint nblocks; // Number of valid blocks
  uint blocknos[8];
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
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk
