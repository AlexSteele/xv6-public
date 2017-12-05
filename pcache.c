#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "buf.h"
#include "mmu.h"

#define NPAGE 32

// Page hash table size
#define NTABLE 61

struct {
  struct spinlock lock;

  // All page cache pages.
  struct page pages[NPAGE];

  // Page hashtable. Pages are hashed by their first
  // block number. Only valid pages are kept in the table.
  struct page *htable[NTABLE];

  // LRU page list. A page in the list may be
  // valid or invalid, but its ref count must be 0.
  struct page *head;
  struct page *tail;
} pcache;

static void lru_push_back(struct page *);

void
pcacheinit(void)
{
  struct page *pp;

  initlock(&pcache.lock, "pcache");
  for (pp = pcache.pages; pp < pcache.pages + NPAGE; pp++) {
    initsleeplock(&pp->lock, "page");
    pp->data = (uchar *) kalloc();
    if (!pp->data) {
      panic("pcacheinit: insufficient pages");
    }
  }

  for (pp = pcache.pages; pp < pcache.pages + NPAGE; pp++) {
    lru_push_back(pp);
  }
}

static uint
hash(uint x)
{
  // Taken from
  //   https://stackoverflow.com/questions/664014/
  //   what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}

static struct page**
hash_find_entry(uint phash, uint start_block)
{
  uint idx = phash % NTABLE;
  while (pcache.htable[idx] &&
         pcache.htable[idx]->blocknos[0] != start_block) {
    idx = (idx + 1) % NTABLE;
  }
  return &pcache.htable[idx];
}

static void
hash_add(uint phash, struct page *pp)
{
  struct page **entry;

  entry = hash_find_entry(phash, pp->blocknos[0]);
  if (*entry)
    panic("pcache: page already in htable");
  *entry = pp;
}

static void
hash_remove(uint phash, uint start_block)
{
  struct page **entry;

  entry = hash_find_entry(phash, start_block);
  *entry = 0;
}

static struct page *
hash_find(uint phash, uint start_block)
{
  struct page **entry;

  entry = hash_find_entry(phash, start_block);
  return *entry;
}

static void
lru_push_front(struct page *pp)
{
  if (!pcache.head) {
    pcache.head = pp;
    pcache.tail = pp;
    return;
  }

  pp->next = pcache.head;
  pcache.head->prev = pp;
  pcache.head = pp;
}

static void
lru_push_back(struct page *pp)
{
  if (!pcache.head) {
    pcache.head = pp;
    pcache.tail = pp;
    return;
  }

  pp->prev = pcache.tail;
  pcache.tail->next = pp;
  pcache.tail = pp;
}

static void
lru_remove(struct page *pp)
{
  if (pp == pcache.head)
    pcache.head = pp->next;
  if (pp == pcache.tail)
    pcache.tail = pp->prev;

  if (pp->next)
    pp->next->prev = pp->prev;
  if (pp->prev)
    pp->prev->next = pp->next;

  pp->next = 0;
  pp->prev = 0;
}

static struct page*
lru_pop(void)
{
  struct page *pp;

  pp = pcache.head;
  if (!pp)
    return 0;
  if (pp == pcache.tail)
    pcache.tail = 0;
  if (pp->next)
    pp->next->prev = 0;

  pcache.head = pp->next;
  pp->next = 0;
  pp->prev = 0;

  return pp;
}

static struct page *
lookup(struct inode *ip, uint off)
{
  uint start_block;
  struct page *pp;
  int i;
  int h;

  off = PGROUNDDOWN(off);
  start_block = bmap(ip, off/BSIZE);
  acquire(&pcache.lock);

  if (ip->mrupage &&
      ip->mrupage->nblocks > 0 &&
      ip->mrupage->blocknos[0] == start_block) {
    lru_remove(ip->mrupage);
    ip->mrupage->refcnt++;
    release(&pcache.lock);
    acquiresleep(&ip->mrupage->lock);
    return ip->mrupage;
  }

  h = hash(start_block);
  if ((pp = hash_find(h, start_block)) != 0) {
    lru_remove(pp);
    pp->refcnt++;
    release(&pcache.lock);
    acquiresleep(&pp->lock);
    return pp;
  }

  pp = lru_pop();
  if (!pp)
    panic("pcache: out of pages");
  if (pp->nblocks > 0)
    hash_remove(hash(pp->blocknos[0]), pp->blocknos[0]);

  // Must pin blocknos[0] before releasing pcache.lock
  pp->blocknos[0] = start_block;
  pp->refcnt = 1;

  hash_add(h, pp);

  release(&pcache.lock);
  acquiresleep(&pp->lock);

  // Reset flags and pin the file's blocks to the page.
  off += BSIZE;
  for (i = 1; i < 8 && off / BSIZE < MAXFILE; i++) {
    pp->blocknos[i] = bmap(ip, off/BSIZE);
    pp->flags[i] = 0;
    off += BSIZE;
  }
  pp->nblocks = i;
  return pp;
}

// Synchronize block of pp with the disk.
// Caller must hold pp->lock.
static void
pagerw(struct page *pp, int block)
{
  struct buf b;

  b.dev = 1;
  b.blockno = pp->blocknos[block];
  b.flags = pp->flags[block];
  b.data = pp->data + (block * BSIZE);
  iderw(&b);
  pp->flags[block] |= B_VALID;
  pp->flags[block] &= ~B_DIRTY;
}

// Find or allocate a page in the page cache for the
// given file and file offset.
// ip->lock must be held and ip must be valid.
struct page *
find_page(struct inode *ip, uint off)
{
  struct page *pp;

  pp = lookup(ip, off);
  ip->mrupage = pp;
  return pp;
}

// Read a portion of a page from disk if necessary.
// pp->lock must be held.
void
read_page(struct page *pp, uint start, uint end)
{
  uint start_block = start / BSIZE;
  uint end_block = (end + BSIZE - 1) / BSIZE;
  int block;

  for (block = start_block; block < end_block; block++) {
    if ((pp->flags[block] & B_VALID) == 0) {
      pagerw(pp, block);
    }
  }
}

// Write a portion of a page to disk.
// pp->lock must be held.
void
write_page(struct page *pp, uint start, uint end)
{
  uint start_block = start / BSIZE;
  uint end_block = (end + BSIZE - 1) / BSIZE;
  int block;

  for (block = start_block; block < end_block; block++) {
    pp->flags[block] |= B_DIRTY;
    pagerw(pp, block);
  }
}

void
release_page(struct page *pp)
{
  if (!holdingsleep(&pp->lock)) {
    panic("release_page");
  }
  releasesleep(&pp->lock);
  acquire(&pcache.lock);
  pp->refcnt--;
  if (pp->refcnt == 0)
    lru_push_back(pp);
  release(&pcache.lock);
}

// Evict all cached pages for a file.
void
evict_pages(struct inode *ip)
{
  int off;
  int blockno;
  struct page **hash_entry;
  struct page *pp;

  for (off = 0; off < ip->size; off += PGSIZE) {
    blockno = bmap(ip, off/BSIZE);
    acquire(&pcache.lock);
    hash_entry = hash_find_entry(hash(blockno), blockno);
    if ((pp = *hash_entry)) {
      if (pp->refcnt > 0)
        panic("evict_pages: page has active refs");

      // The page is cached.
      // Evict it and place it the at head of LRU list.
      pp->nblocks = 0;
      lru_remove(pp);
      lru_push_front(pp);
      *hash_entry = 0;

    }
    release(&pcache.lock);
  }
}
