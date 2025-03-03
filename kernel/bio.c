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

#define BACKET_HASH(dev, blk) (((dev << 27) | blk) % BUCKET_SIZE)

struct {
  // struct spinlock lock;

  struct spinlock bhash_lock[BUCKET_SIZE];
  struct buf bhash_head[BUCKET_SIZE];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  for (int i = 0; i < BUCKET_SIZE; i++) {
    initlock(&bcache.bhash_lock[i], "bcache buf lock");
    bcache.bhash_head[i].next = 0;
  }

  for (int i = 0; i < NBUF; i++) {  // 把所有的缓存分配到bucket 0上
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buf sleep lock");
    b->lastuse = 0;
    b->refcnt = 0;
    b->next = bcache.bhash_head[0].next;
    bcache.bhash_head[0].next = b;
  }
}

struct buf* bfind_prelru(int* lru_bkt) {
  struct buf* lru_res = 0;
  *lru_bkt = -1;
  struct buf* b;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    acquire(&bcache.bhash_lock[i]);
    uint found = 0;
    for (b = &bcache.bhash_head[i]; b->next; b = b->next) {
      if (b->next->refcnt == 0 && (!lru_res || b->next->lastuse < lru_res->lastuse)) {
        lru_res = b;
        found = 1;
      }
    }

    if (found) {
      if (*lru_bkt != -1) {
        release(&bcache.bhash_lock[*lru_bkt]);
      }
      *lru_bkt = i;
    }
    else {
      release(&bcache.bhash_lock[i]);
    }
  }

  return lru_res;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BACKET_HASH(dev, blockno);

  acquire(&bcache.bhash_lock[key]);

  // Is the block already cached?
  for(b = bcache.bhash_head[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bhash_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bhash_lock[key]);
  int lru_bkt;
  struct buf* pre_lru = bfind_prelru(&lru_bkt);
  if (pre_lru == 0) {
    panic("bget: no buffers\n");
  }

  struct buf* lru = pre_lru->next;
  pre_lru->next = lru->next;
  release(&bcache.bhash_lock[lru_bkt]);

  acquire(&bcache.bhash_lock[key]);
  for(b = bcache.bhash_head[key].next; b; b = b->next){
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bhash_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  lru->next = bcache.bhash_head[key].next;
  bcache.bhash_head[key].next = lru;

  lru->dev = dev;
  lru->blockno = blockno;
  lru->valid = 0;
  lru->refcnt = 1;

  release(&bcache.bhash_lock[key]);
  acquiresleep(&lru->lock);

  return lru;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BACKET_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse = ticks;
  }
  
  release(&bcache.bhash_lock[key]);
}

void
bpin(struct buf *b) {
  uint key = BACKET_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lock[key]);
  b->refcnt++;
  release(&bcache.bhash_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = BACKET_HASH(b->dev, b->blockno);
  acquire(&bcache.bhash_lock[key]);
  b->refcnt--;
  release(&bcache.bhash_lock[key]);
}


