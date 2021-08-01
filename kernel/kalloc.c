// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void initfree(int id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

uint64 range = 0; // range for per core.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmemlist[NCPU];

void
kinit()
{
  char kmemname[5] = "kmem0";
  range = (PHYSTOP - PGROUNDUP((uint64)end)) / NCPU;
  for(int id = 0;id < NCPU;id++){
    kmemname[5] = id + '0';
    initlock(&kmemlist[id].lock, kmemname);
    initfree(id);
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;

  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id;

  push_off();
  id = cpuid();
  pop_off();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP){
    if (((uint64)pa % PGSIZE) != 0)
      printf("pa is not aligned\n");
    if ((char*)pa < end)
      printf("pa is too small\n");
    if ((uint64)pa >= PHYSTOP)
      printf("pa is too large\n");
    panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmemlist[id].lock);
  r->next = kmemlist[id].freelist;
  kmemlist[id].freelist = r;
  release(&kmemlist[id].lock);
}

void
initfree(int id)
{
  struct run *r;
  char *p = (char*)(PGROUNDUP((uint64)end) + PGROUNDDOWN((uint64)(range*id)));
  char *pa_end = (id == NCPU - 1) ? (char*)PHYSTOP :p + range;

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    memset(p, 1, PGSIZE);
    acquire(&kmemlist[id].lock);
    r = (struct run*)p;
    r->next = kmemlist[id].freelist;
    kmemlist[id].freelist = r;
    release(&kmemlist[id].lock);
  }
}
    

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id;
  int i;

  push_off();
  id = cpuid();
  pop_off();

  acquire(&kmemlist[id].lock);
  r = kmemlist[id].freelist;
  if(r){
    kmemlist[id].freelist = r->next;
  } else if (r == 0){
    for(i = 1; i < NCPU; i++){
      if(kmemlist[(id+i)%NCPU].freelist) {
        r = kmemlist[(id+i)%NCPU].freelist;
        kmemlist[(id+i)%NCPU].freelist = r->next;
        break;
      }
    }
    if(i == NCPU){
      release(&kmemlist[id].lock);
      return 0;
    }
  }
  release(&kmemlist[id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
