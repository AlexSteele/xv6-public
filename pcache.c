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

/* TODO: Does nblocks ever get reset to 0? */

struct {
  struct spinlock lock;

  // All page cache pages.
  struct page pages[NPAGE];

  // Page hashtable
  struct page *htable[NTABLE];

  // LRU page list
  struct page *head;
  struct page *tail;
} pcache;

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
  
  // Create LRU list
  pcache.head = &pcache.pages[0];
  pcache.pages[0].next = &pcache.pages[1];
  for (pp = pcache.pages + 1; pp < pcache.pages + NPAGE - 1; pp++) {
    pp->prev = pp - 1;
    pp->next = pp + 1;
  }
  pcache.tail = pp;
  pp->prev = pp - 1;
  pp->next = 0;
}

static uint
hash(uint v)
{
  // TODO: Better hash :)
  return v;
}

static void
hash_add(uint phash, struct page *pp)
{
  uint idx = phash % NTABLE;
  while (pcache.htable[idx]) {
    idx = (idx + 1) % NTABLE;
  }
  pcache.htable[idx] = pp;
}

static void
hash_remove(uint phash, uint start_block)
{
  uint idx = phash % NTABLE;
  while (pcache.htable[idx] &&
         pcache.htable[idx]->blocknos[0] != start_block) {
    idx = (idx + 1) % NTABLE;
  }
  pcache.htable[idx] = 0;
}

static struct page *
hash_find(uint phash, uint start_block)
{
  uint idx = phash % NTABLE;
  while (pcache.htable[idx] &&
         pcache.htable[idx]->blocknos[0] != start_block) {
    idx = (idx + 1) % NTABLE;
  }
  return pcache.htable[idx];
}

static void
lru_put(struct page *pp)
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

static struct  page*
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
  hash_add(h, pp);
  pp->refcnt = 1;
  release(&pcache.lock);
  acquiresleep(&pp->lock);

  // Reset flags and pin the file's blocks to the page.
  pp->flags = 0;
  pp->blocknos[0] = start_block;
  off += BSIZE;
  for (i = 1; i < 8 && off / BSIZE < MAXFILE; i++) {
    pp->blocknos[i] = bmap(ip, off/BSIZE);
    off += BSIZE;
  }
  pp->nblocks = i;
  return pp;
}

// Synchronize a page region with the disk.
// Caller must hold pp->lock.
static void
pagerw(struct page *pp, uint start, uint end)
{
  uint start_block = start / BSIZE;
  uint end_block = (end + BSIZE - 1) / BSIZE;
  int i;

  for (i = start_block; i < end_block; i++) {
    struct buf b;
    memset(&b, 0, sizeof(b));
    b.dev = 1;
    b.flags = pp->flags;
    b.blockno = pp->blocknos[i];
    b.data = pp->data + (i * BSIZE);
    iderw(&b);
  }

  // Update the page's flags
  pp->flags |= B_VALID;
  pp->flags &= ~B_DIRTY;
}

// Find or allocate a page in the page cache
// corresponding to the given file and file offset.
// ip->lock must be held and ip must be valid.
struct page *
find_page(struct inode *ip, uint off)
{
  struct page *pp;

  pp = lookup(ip, off);
  if ((pp->flags & B_VALID) == 0) {
    pagerw(pp, 0, BSIZE*pp->nblocks);
  }
  ip->mrupage = pp;
  return pp;
}

// Write page contents from offset 'start' to offset 'end' to disk.
// pp->lock must be held.
void
write_page(struct page *pp, uint start, uint end)
{
  pp->flags |= B_DIRTY;
  pagerw(pp, start, end);
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
    lru_put(pp);
  release(&pcache.lock);
}
