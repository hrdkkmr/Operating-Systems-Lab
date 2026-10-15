// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

// Lab 8: the allocator also keeps
//  - nfree:  number of pages currently on the free list (Q2),
//  - ntotal: number of pages ever handed to the allocator by kinit1/kinit2,
//  - ref[]:  a reference count for every physical page frame (Q5/Q7).
// A frame returned by kalloc() has ref 1.  Sharing it (copy-on-write
// fork, shared-memory mapping) adds references with kincref(), and kfree()
// drops one reference and puts the frame on the free list only when the
// last one is gone.  So every existing kfree() call site keeps working
// unchanged for pages that are never shared.  All fields are protected by
// kmem.lock once use_lock is set (before that only one CPU runs).
struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  int nfree;
  int ntotal;
  ushort ref[PHYSTOP / PGSIZE];
} kmem;

#define PA2IDX(pa) ((uint)(pa) / PGSIZE)

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE){
    // Lab 8: a new frame enters the allocator as if it had been
    // kalloc()ed once, so that kfree() below takes it to ref 0 / free.
    kmem.ref[PA2IDX(V2P(p))] = 1;
    kmem.ntotal++;
    kfree(p);
  }
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // Lab 8: drop one reference; the frame is freed only by the last one.
  // A frame with no references is already free: catch double frees
  // instead of corrupting the free list and the free-page counter.
  if(kmem.ref[PA2IDX(V2P(v))] == 0)
    panic("kfree: page already free");
  if(--kmem.ref[PA2IDX(V2P(v))] > 0){
    if(kmem.use_lock)
      release(&kmem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.nfree++;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    // Counters change only when a page really leaves the free list,
    // so a failed allocation leaves them untouched.
    kmem.freelist = r->next;
    kmem.nfree--;
    kmem.ref[PA2IDX(V2P(r))] = 1;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// Lab 8 Q5/Q7: add a reference to an allocated frame (physical address).
void
kincref(uint pa)
{
  if(pa % PGSIZE || pa < V2P(end) || pa >= PHYSTOP)
    panic("kincref");
  acquire(&kmem.lock);
  if(kmem.ref[PA2IDX(pa)] == 0)
    panic("kincref: free page");
  kmem.ref[PA2IDX(pa)]++;
  release(&kmem.lock);
}

// Lab 8 Q5: current number of references to the frame at pa.
int
kpageref(uint pa)
{
  int n;

  if(pa >= PHYSTOP)
    return 0;
  acquire(&kmem.lock);
  n = kmem.ref[PA2IDX(pa)];
  release(&kmem.lock);
  return n;
}

// Lab 8 Q2: snapshot of the allocator counters.
void
kmemstat(int *total, int *nfree)
{
  acquire(&kmem.lock);
  *total = kmem.ntotal;
  *nfree = kmem.nfree;
  release(&kmem.lock);
}

