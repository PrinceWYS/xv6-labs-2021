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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 candidates[STEAL_CNT];
} kmem[NCPU];

char kmem_lock_name[NCPU][sizeof("kmem cpu 0")];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmem_lock_name[i], sizeof("kmem cpu 0"), "kmem cpu %d", i);
    initlock(&kmem[i].lock, kmem_lock_name[i]);
  }
  freerange(end, (void*)PHYSTOP);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int idx = cpuid();
  pop_off();

  acquire(&kmem[idx].lock);
  r->next = kmem[idx].freelist;
  kmem[idx].freelist = r;
  release(&kmem[idx].lock);
}

int steal(int cpu) {
  uint steal_left = STEAL_CNT;
  int idx = 0;

  memset(kmem[cpu].candidates, 0, sizeof(kmem[cpu].candidates));
  for (int i = 0; i < NCPU; i++) {
    if (i == cpu) { // 自身
      continue;
    }

    acquire(&kmem[i].lock);

    while(kmem[i].freelist && steal_left) {
      kmem[cpu].candidates[idx++] = (uint64)kmem[i].freelist;
      kmem[i].freelist = kmem[i].freelist->next;
      steal_left--;
    }

    release(&kmem[i].lock);
    if (steal_left == 0) {
      break;
    }
  }

  return idx;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int idx = cpuid();

  acquire(&kmem[idx].lock);
  r = kmem[idx].freelist;
  if(r) {
    kmem[idx].freelist = r->next;
    release(&kmem[idx].lock);
  }
  else {
    release(&kmem[idx].lock);
    int ret = steal(idx);
    if (ret > 0) {
      acquire(&kmem[idx].lock);
      for (int i = 0; i < ret; i++) {
        if (!kmem[idx].candidates[i]) {
          break;
        }
        ((struct run *)kmem[idx].candidates[i])->next = kmem[idx].freelist;
        kmem[idx].freelist = (struct run *)kmem[idx].candidates[i];
      }
      r = kmem[idx].freelist;
      kmem[idx].freelist = r->next;
      release(&kmem[idx].lock);
    }
    else {
      pop_off();
      return 0;
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
