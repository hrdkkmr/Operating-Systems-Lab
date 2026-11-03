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

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Lab 10 Q1: global counters for physical-page allocation tracking.
// pages_allocated counts every successful kalloc() after boot.
// pages_freed    counts every kfree() that is not part of kinit's free-range
//                (use_lock is 0 during kinit, so we skip those increments).
static uint pages_allocated = 0;
static uint pages_freed     = 0;

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
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  // Lab 10 Q1: only count post-boot frees (use_lock is 1 after kinit2).
  if(kmem.use_lock)
    pages_freed++;
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
    kmem.freelist = r->next;
    // Lab 10 Q1: only count post-boot allocations (use_lock is 1 after kinit2).
    if(kmem.use_lock)
      pages_allocated++;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// Lab 10 Q1: count free pages on the freelist.
static uint
freelist_count(void)
{
  struct run *r;
  uint n = 0;
  r = kmem.freelist;
  while(r){ r = r->next; n++; }
  return n;
}

// Lab 10 Q1: fill *allocated, *freed, *current, *avail and print a summary.
// Called from sys_memstats() in sysproc.c.
void
kmemstats(uint *allocated, uint *freed, uint *current, uint *avail)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);
  *allocated = pages_allocated;
  *freed     = pages_freed;
  *current   = (pages_allocated > pages_freed) ? (pages_allocated - pages_freed) : 0;
  *avail     = freelist_count();
  if(kmem.use_lock)
    release(&kmem.lock);

  cprintf("=== Physical Memory Statistics ===\n");
  cprintf("  Total allocated pages : %d\n", *allocated);
  cprintf("  Total freed pages     : %d\n", *freed);
  cprintf("  Currently in use      : %d\n", *current);
  cprintf("  Available free pages  : %d\n", *avail);
  cprintf("==================================\n");
}
