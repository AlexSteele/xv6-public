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

struct {
  struct spinlock lock;
  struct page pages[NPAGE];
} pcache;

void
pcacheinit(void)
{
  struct page *p;

  initlock(&pcache.lock, "pcache");
  for (p = pcache.pages; p < pcache.pages + NPAGE; p++) {
    initsleeplock(&p->lock, "page");
    p->data = (uchar *) kalloc();
    if (!p->data) {
      panic("pcacheinit: insufficient pages");
    }
  }
}

static struct page *
_find_page(struct inode *ip, uint off)
{
  uint start_block;
  struct page *pp;
  int i;

  off = PGROUNDDOWN(off);

  start_block = bmap(ip, off/BSIZE);

  acquire(&pcache.lock);

  // Check if page is already cached
  for (pp = pcache.pages; pp < pcache.pages + NPAGE; pp++) {
    if (pp->blocknos[0] == start_block) {
      pp->refcnt++;
      release(&pcache.lock);
      acquiresleep(&pp->lock);
      return pp;
    }
  }

  // Otherwise, find an unreferenced page
  for (pp = pcache.pages; pp < pcache.pages + NPAGE; pp++) {
    if (pp->refcnt == 0 && (pp->flags & B_DIRTY) == 0) {
      break;
    }
  }
  if (pp == pcache.pages + NPAGE) {
    panic("find_page: no free pages");
  }

  // Update refcnt and release pcache lock before
  // pinning the file's blocks to avoid deadlock
  // since bmap() may call iderw() to read ip's 
  // indirect block from disk. iderw() sleeps so
  // we can't hold pcache.lock when it's called.
  pp->refcnt = 1;
  release(&pcache.lock);
  acquiresleep(&pp->lock);

  pp->flags = 0;

  // Pin the file's blocks to the page
  pp->blocknos[0] = start_block;
  off += BSIZE;
  for (i = 1; i < 8 && off < ip->size; i++) {
    pp->blocknos[i] = bmap(ip, off/BSIZE);
    off += BSIZE;
  }
  pp->nblocks = i;
  return pp;
}

// Synchronize a page with the disk.
// Caller must hold p->lock.
static void
pagerw(struct page *p)
{
  int i;

  // Synchronize each block
  for (i = 0; i < p->nblocks; i++) {
    struct buf b;
    memset(&b, 0, sizeof(b));
    b.dev = 1;
    b.flags = p->flags;
    b.blockno = p->blocknos[i];
    b.data = p->data + (i * BSIZE);
    iderw(&b);
  }

  // Update the page's flags
  p->flags |= B_VALID;
  p->flags &= ~B_DIRTY;
}

struct page *
find_page(struct inode *ip, uint off)
{
  struct page *p;

  p = _find_page(ip, off);
  if ((p->flags & B_VALID) == 0) {
    pagerw(p);
  }
  return p;
}

// Read a page's pinned blocks from disk.
// p->lock must be held.
void
read_page(struct page *p)
{
  panic("not implemented");
}

// Write a page to disk. p->lock must be held.
void
write_page(struct page *p)
{
  p->flags |= B_DIRTY;
  pagerw(p);  
}

void
release_page(struct page *p)
{
  if (!holdingsleep(&p->lock)) {
    panic("release_page");
  }
  releasesleep(&p->lock);
  acquire(&pcache.lock);
  p->refcnt--;
  release(&pcache.lock);
}
