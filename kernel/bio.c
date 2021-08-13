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

void printLRU();
void printHash();

void updateLRU(struct buf *buf);

// hash bucket
struct hbuc {
  struct spinlock lock; // lock per bucket
  struct buf *head; // buffers in this bucket
  struct buf *tail;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  // lruhead for the Least-Recently Used list
  struct buf lruhead;
} bcache;

struct hbuc hbuc[NBUC];

void
binit(void)
{
  struct buf *b, *tmp = 0;

  initlock(&bcache.lock, "bcache");

  bcache.lruhead.num = 0;
  for(int i = 0; i < NBUC; i++){
    initlock(&hbuc[i].lock, "bcache.bucket");
  }

  // Create linked list of buffers
  int i = 1;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++, i++){
    b->bucnext = 0;
    b->bucprev = 0;
    b->num = i;
    if(tmp == 0){
      tmp = b;
      bcache.lruhead.lruprev = b;
      b->lrunext = &bcache.lruhead;
      continue;
    }
    b->lrunext = tmp;
    tmp->lruprev = b;
    tmp = b;
    initsleeplock(&b->lock, "buffer");
  }
  bcache.lruhead.lrunext = tmp;
  tmp->lruprev = &bcache.lruhead;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucidx = blockno % NBUC;

  printf("block to be cached is %d\n", blockno);
  acquire(&hbuc[bucidx].lock);
  // Is the block already cached?
  for(b = hbuc[bucidx].head; b != 0; b = b->bucnext){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&hbuc[bucidx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&hbuc[bucidx].lock);
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  acquire(&bcache.lock);
  acquire(&hbuc[bucidx].lock);

  while(1){
    // printLRU();
    for(b = bcache.lruhead.lruprev; b != &bcache.lruhead; b = b->lruprev){
      // Search a new cache currently idle
      if(b->refcnt == 0) {
        // If it's been used, delete it from the old hash bucket
        int oldblockno= b->blockno, oldbucidx = b->blockno%NBUC; 
        // printHash();
        if(oldblockno != 0 && oldbucidx != bucidx){
          acquire(&hbuc[oldbucidx].lock);
          // If other process used the same block, reselect a block
          if(b->refcnt != 0){
            // printf("change between, b->refcnt is %d\n", b->refcnt);
            release(&hbuc[oldbucidx].lock);
            continue;
          }
          if(b->bucnext && b->bucprev){
            b->bucprev->bucnext = b->bucnext;
            b->bucnext->bucprev = b->bucprev;
            b->bucprev = 0;
            b->bucnext = 0;
          } else if(b->bucnext){
            hbuc[oldbucidx].head = b->bucnext;
            b->bucnext->bucprev = 0; 
            b->bucnext = 0;
          } else if(b->bucprev){
            hbuc[oldbucidx].tail = b->bucprev;
            b->bucprev->bucnext = 0;
            b->bucprev = 0;
          } else{
            hbuc[oldbucidx].head = 0;
            hbuc[oldbucidx].tail = 0;
          }
        }
        // printHash();
        // If not in the same bucket, insert it to the new hash bucket
        if(b->blockno == 0 || b->blockno%NBUC != bucidx){
          if(hbuc[bucidx].head){
            hbuc[bucidx].tail->bucnext = b;
            b->bucprev = hbuc[bucidx].tail;
            b->bucnext = 0;
            hbuc[bucidx].tail = b;
          } else{
            hbuc[bucidx].head = b;
            hbuc[bucidx].tail = b;
          }
        }
  
        b->blockno = blockno;
        b->dev = dev;
        b->valid = 0;
        b->refcnt = 1;
  
        // update lrulist
        updateLRU(b);
  
        // printHash();
        // printLRU();
        release(&bcache.lock);
        release(&hbuc[bucidx].lock);
        if(oldblockno != 0 && oldbucidx != bucidx) {
          release(&hbuc[oldbucidx].lock);
        }
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
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
void
brelse(struct buf *b)
{
  int idx = b->blockno%NBUC;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&hbuc[idx].lock);
  // acquire(&bcache.lock);
  b->refcnt--;
  // release(&bcache.lock);
  release(&hbuc[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = b->blockno%NBUC;
  acquire(&hbuc[idx].lock);
  // acquire(&bcache.lock);
  b->refcnt++;
  // release(&bcache.lock);
  release(&hbuc[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = b->blockno%NBUC;
  acquire(&hbuc[idx].lock);
  // acquire(&bcache.lock);
  b->refcnt--;
  // release(&bcache.lock);
  release(&hbuc[idx].lock);
}

// After a buffer is used(get ), put it on the head->next of LRU list
void
updateLRU(struct buf *b){
  //fetch b from lru list
  b->lruprev->lrunext = b->lrunext;
  b->lrunext->lruprev = b->lruprev;
  bcache.lruhead.lrunext->lruprev = b;
  b->lrunext = bcache.lruhead.lrunext;
  b->lruprev = &bcache.lruhead;
  bcache.lruhead.lrunext = b;
  return;
}

// For debug
void
printLRU(){
  int i = 1;
  struct buf *b;
  printf("\n");
  for(b = bcache.lruhead.lruprev; b != &bcache.lruhead; b = b->lruprev, i++){
    printf("lru %d block is %d \t refcnt is %d \t prev is: %d \t next is: %d \t dev is: %d \t blockno is: %d \t ", i, b->num, b->refcnt, b->lruprev->num, b->lrunext->num, b->dev, b->blockno);
    if(b->bucprev)
      printf("bucprev is %d \t ", b->bucprev->num);
    if(b->bucnext)
      printf("bucnext is %d \t ", b->bucnext->num);
    printf("\n");
  }
  printf("\n");
}

void
printHash(){
  printf("\n");
  for(int i = 0; i < NBUC; i++){
    printf("%dth is: ", i);
    for(struct buf *head = hbuc[i].head; head != 0; head = head->bucnext){
      printf("%d ", head->num);
    }
    printf("\n");
  }
  printf("\n");
}
