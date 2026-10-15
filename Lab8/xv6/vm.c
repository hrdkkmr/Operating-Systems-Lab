#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
// Lab 8 Q1: no longer static; exported through defs.h.
pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  // Lab 8: user memory ends at USERTOP (the shared pages live above it).
  if(newsz > USERTOP)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
// Lab 8: pages that were never touched (Q4 lazy allocation) have no PTE
// or a non-present one and are skipped.  For a page shared copy-on-write
// (Q5) or as shared memory (Q7) kfree() only drops this mapping's
// reference; the frame is freed when its last mapping goes away.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Shared-memory mapping helpers.  mappages() itself remains private to vm.c.
int
shm_map(pde_t *pgdir, uint pa)
{
  return mappages(pgdir, (void*)SHMVA, PGSIZE, pa, PTE_W | PTE_U);
}

void
shm_unmap(pde_t *pgdir)
{
  pte_t *pte = walkpgdir(pgdir, (void*)SHMVA, 0);
  if(pte == 0 || !(*pte & PTE_P))
    return;
  *pte = 0;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Lab 8: flush this CPU's TLB if pgdir is the page table it is using.
// (xv6 has no TLB shootdown, see the notes in proc.c.)
static void
flushtlb(pde_t *pgdir)
{
  struct proc *p = myproc();

  if(p && p->pgdir == pgdir)
    lcr3(V2P(pgdir));
}

// Given a parent process's page table, create a copy
// of it for a child.
// Lab 8 Q4: pages of the lazy heap that were never touched have no
// page table or a non-present PTE.  They are skipped (the child gets the
// same untouched lazy pages) instead of panicking.
// Lab 8 Q5: if cow is set, user pages are not copied.  The child maps the
// parent's frame, both PTEs become read-only with PTE_COW set and the
// frame's reference count is incremented; the first write by either
// process copies it (see uvmfault).  Pages without PTE_U (the stack
// guard page) are always copied eagerly, they are never written anyway.
// The parent's PTEs change, so its TLB is flushed.
pde_t*
copyuvm(pde_t *pgdir, uint sz, int cow)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0){
      // No page table: the whole 4 MB slot is untouched lazy heap.
      i = PGADDR(PDX(i) + 1, 0, 0) - PGSIZE;
      continue;
    }
    if(!(*pte & PTE_P))
      continue;   // untouched lazy page
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if(cow && (flags & PTE_U)){
      // Writable (or mprotect'ed, which may be made writable again by
      // munprotect) pages become shared copy-on-write.
      if(flags & (PTE_W | PTE_PROT)){
        flags = (flags & ~PTE_W) | PTE_COW;
        *pte = pa | flags;
      }
      if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0)
        goto bad;
      kincref(pa);
      continue;
    }
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  if(cow)
    flushtlb(pgdir);
  return d;

bad:
  // Pages already shared with d lose d's reference in freevm().  Parent
  // PTEs already marked PTE_COW stay correct: with a reference count of
  // 1 the next write fault simply makes them writable again.
  if(cow)
    flushtlb(pgdir);
  freevm(d);
  return 0;
}

// Lab 8 Q4/Q5/Q6: resolve a fault (or an intended kernel access) on user
// address va in pgdir.  Does not lock: callers hold ptable.lock when
// pgdir can be shared with other threads (see pagefault() in proc.c) or
// own a private pgdir (exec).
//  - Page not present and lo <= va < hi: demand allocation (Q4): a zeroed
//    frame is mapped PTE_W|PTE_U.  lo is the start of the heap (just above
//    the stack), hi the process size.
//  - Present user page, write access, PTE_COW set (Q5): if the frame is
//    still shared, copy it into a new frame; if this is the last
//    reference, just make it writable again.
// Returns 0 when the access may now proceed, or a negative FAULT_* code.
int
uvmfault(pde_t *pgdir, uint va, uint lo, uint hi, int write)
{
  pte_t *pte;
  uint a, pa, flags;
  char *mem;

  if(va >= KERNBASE)
    return FAULT_RANGE;
  a = PGROUNDDOWN(va);
  pte = walkpgdir(pgdir, (char*)a, 0);
  if(pte == 0 || (*pte & PTE_P) == 0){
    if(va < lo || va >= hi)
      return FAULT_RANGE;
    if((mem = kalloc()) == 0)
      return FAULT_NOMEM;
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      kfree(mem);   // no page-table page: nothing was mapped
      return FAULT_NOMEM;
    }
    return 0;
  }
  if((*pte & PTE_U) == 0)
    return FAULT_NOTUSER;
  if(!write || (*pte & PTE_W)){
    // Accessible already, e.g. another thread resolved it first or a
    // stale TLB entry caused the fault.
    flushtlb(pgdir);
    return 0;
  }
  if(*pte & PTE_PROT)
    return FAULT_PROT;
  if((*pte & PTE_COW) == 0)
    return FAULT_READONLY;

  pa = PTE_ADDR(*pte);
  flags = (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW;
  if(kpageref(pa) > 1){
    if((mem = kalloc()) == 0)
      return FAULT_NOMEM;
    memmove(mem, P2V(pa), PGSIZE);
    *pte = V2P(mem) | flags;
    kfree(P2V(pa));          // drop our reference to the shared frame
  } else {
    *pte = pa | flags;       // last reference: take the frame over
  }
  flushtlb(pgdir);
  return 0;
}

// Lab 8: human-readable reason for a negative uvmfault() result.
char*
faultreason(int code)
{
  switch(code){
  case FAULT_RANGE:    return "address outside the valid heap/user range";
  case FAULT_NOMEM:    return "out of physical memory";
  case FAULT_NOTUSER:  return "page not user-accessible (PTE_U clear, e.g. stack guard page)";
  case FAULT_PROT:     return "write to a page made read-only by mprotect()";
  case FAULT_READONLY: return "write to a read-only page";
  }
  return "unknown";
}

// Lab 8: may user address va be accessed right now without faulting?
// (present, PTE_U, and PTE_W for a write).  Lock-free fast path for the
// system-call argument checks in syscall.c.
int
uvmaccessok(pde_t *pgdir, uint va, int write)
{
  pte_t *pte;

  if(va >= KERNBASE)
    return 0;
  if((pgdir[PDX(va)] & PTE_P) == 0)
    return 0;
  pte = walkpgdir(pgdir, (char*)va, 0);
  if(pte == 0 || (*pte & (PTE_P|PTE_U)) != (PTE_P|PTE_U))
    return 0;
  if(write && (*pte & PTE_W) == 0)
    return 0;
  return 1;
}

// Lab 8 Q1: translate user virtual address va to a physical address in
// pgdir, printing the page directory index, page table index, offset and
// the P/W/U bits.  Returns -1 if the page is not present or does not have
// PTE_U.  Never dereferences a page table whose PDE is not present.
int
va2pa(pde_t *pgdir, uint va)
{
  pde_t pde;
  pte_t *pte;
  uint pa;

  pde = pgdir[PDX(va)];
  cprintf("[va2pa] va 0x%x: PDX %d, PTX %d, offset 0x%x\n",
          va, PDX(va), PTX(va), va & (PGSIZE-1));
  if((pde & PTE_P) == 0){
    cprintf("[va2pa]   page directory entry not present -> -1\n");
    return -1;
  }
  pte = walkpgdir(pgdir, (char*)va, 0);
  cprintf("[va2pa]   PDE 0x%x (page table pa 0x%x), PTE 0x%x: P=%d W=%d U=%d\n",
          pde, PTE_ADDR(pde), *pte, (*pte & PTE_P) != 0,
          (*pte & PTE_W) != 0, (*pte & PTE_U) != 0);
  if((*pte & PTE_P) == 0){
    cprintf("[va2pa]   page not present -> -1\n");
    return -1;
  }
  if((*pte & PTE_U) == 0){
    cprintf("[va2pa]   page not user-accessible (PTE_U clear) -> -1\n");
    return -1;
  }
  pa = PTE_ADDR(*pte) | (va & (PGSIZE-1));
  cprintf("[va2pa]   va 0x%x -> frame 0x%x -> pa 0x%x\n",
          va, PTE_ADDR(*pte), pa);
  return pa;
}

static char*
flagstr(uint f, uint bit, char *s)
{
  return (f & bit) ? s : "-";
}

// Lab 8 Q3: dump the user half of pgdir.  Two levels:
//   PDE <index>: pt pa <page table frame> [P W U]
//       PTE <index>: va <va> -> pa <frame> [P W U] <notes>
// Page tables are reached through P2V() of the physical address stored
// in the PDE, and only PDEs with PTE_P are followed.  Kernel PDEs
// (index >= PDX(KERNBASE) = 512) are identical in every process and are
// only counted.  sz is the process size, used for the summary lines.
void
vmprint(pde_t *pgdir, uint sz)
{
  uint i, j, va, f;
  pte_t *pgtab;
  int upages = 0, guard = 0, kpde = 0, kpte = 0;

  cprintf("page directory at pa 0x%x (process sz 0x%x = %d pages)\n",
          V2P(pgdir), sz, sz / PGSIZE);
  for(i = 0; i < PDX(KERNBASE); i++){
    if((pgdir[i] & PTE_P) == 0)
      continue;
    f = pgdir[i];
    pgtab = (pte_t*)P2V(PTE_ADDR(f));
    cprintf(" PDE %d: va 0x%x-0x%x  page table pa 0x%x  [%s %s %s]\n",
            i, PGADDR(i, 0, 0), PGADDR(i, 0, 0) + (PGSIZE << 10) - 1,
            PTE_ADDR(f), flagstr(f, PTE_P, "P"), flagstr(f, PTE_W, "W"),
            flagstr(f, PTE_U, "U"));
    for(j = 0; j < NPTENTRIES; j++){
      f = pgtab[j];
      if((f & PTE_P) == 0)
        continue;
      va = PGADDR(i, j, 0);
      cprintf("     PTE %d: va 0x%x -> pa 0x%x  [%s %s %s]",
              j, va, PTE_ADDR(f), flagstr(f, PTE_P, "P"),
              flagstr(f, PTE_W, "W"), flagstr(f, PTE_U, "U"));
      if(f & PTE_U)
        upages++;
      if((f & PTE_U) == 0 && va < sz){
        guard++;
        cprintf("  <- stack guard page (PTE_U clear)");
      }
      if(f & PTE_COW)
        cprintf("  COW");
      if(f & PTE_PROT)
        cprintf("  mprotect");
      if(va >= USERTOP)
        cprintf("  shared-memory page");
      cprintf("\n");
    }
  }
  for(i = PDX(KERNBASE); i < NPDENTRIES; i++){
    if((pgdir[i] & PTE_P) == 0)
      continue;
    kpde++;
    pgtab = (pte_t*)P2V(PTE_ADDR(pgdir[i]));
    for(j = 0; j < NPTENTRIES; j++)
      if(pgtab[j] & PTE_P)
        kpte++;
  }
  cprintf("user: %d present pages with PTE_U, %d guard page(s); "
          "sz/PGSIZE = %d, sz/PGSIZE - guard = %d\n",
          upages, guard, sz / PGSIZE, sz / PGSIZE - guard);
  cprintf("kernel: %d valid PDEs at index >= %d (va >= KERNBASE) mapping "
          "%d pages, not printed\n", kpde, PDX(KERNBASE), kpte);
}

// Lab 8 Q2: page-table usage and resident user pages of pgdir.
//  ptpages:  the page directory itself plus every page-table page it
//            points to (user and kernel half; xv6 builds a private copy
//            of the kernel page tables for every process in setupkvm).
//  ptuser:   page-table pages covering the user half (va < KERNBASE).
//  resident: present user pages (PTE_U) below KERNBASE.
//  shared:   of those, frames whose reference count is > 1 (Q5/Q7).
void
uvmstat(pde_t *pgdir, int *ptpages, int *ptuser, int *resident, int *shared)
{
  uint i, j;
  pte_t *pgtab;

  *ptpages = 1;
  *ptuser = *resident = *shared = 0;
  for(i = 0; i < NPDENTRIES; i++){
    if((pgdir[i] & PTE_P) == 0)
      continue;
    (*ptpages)++;
    if(i >= PDX(KERNBASE))
      continue;
    (*ptuser)++;
    pgtab = (pte_t*)P2V(PTE_ADDR(pgdir[i]));
    for(j = 0; j < NPTENTRIES; j++){
      if((pgtab[j] & (PTE_P|PTE_U)) != (PTE_P|PTE_U))
        continue;
      (*resident)++;
      if(kpageref(PTE_ADDR(pgtab[j])) > 1)
        (*shared)++;
    }
  }
}

// Lab 8 Q6: make npages pages starting at the page-aligned va read-only
// (protect != 0) or writable again (protect == 0).  The caller has
// checked the range against the process size and holds ptable.lock.
// Untouched lazy pages are allocated first, so every page in the range
// is present afterwards; pages without PTE_U (guard page) are rejected.
// munprotect() leaves a page that is still shared copy-on-write
// read-only: its next write fault gives the process a private copy.
int
uvmprotect(pde_t *pgdir, uint va, uint npages, uint lo, uint hi, int protect)
{
  uint a, end;
  pte_t *pte;

  end = va + npages * PGSIZE;
  for(a = va; a < end; a += PGSIZE)
    if(uvmfault(pgdir, a, lo, hi, 0) < 0)
      return -1;
  for(a = va; a < end; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(protect){
      *pte = (*pte & ~PTE_W) | PTE_PROT;
    } else if(*pte & PTE_PROT){
      *pte &= ~PTE_PROT;
      if((*pte & PTE_COW) == 0)
        *pte |= PTE_W;
    }
  }
  flushtlb(pgdir);
  return 0;
}

// Lab 8 Q7: map the existing frame pa at user address va (outside the
// heap) and take a reference on it.  Fails if va is already mapped.
int
uvmmapshared(pde_t *pgdir, uint va, uint pa)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, (char*)va, 0);
  if(pte && (*pte & PTE_P))
    return -1;
  if(mappages(pgdir, (char*)va, PGSIZE, pa, PTE_W|PTE_U) < 0)
    return -1;
  kincref(pa);
  return 0;
}

// Lab 8 Q7: remove the mapping at va and drop its frame reference.
void
uvmunmapshared(pde_t *pgdir, uint va)
{
  pte_t *pte;
  uint pa;

  pte = walkpgdir(pgdir, (char*)va, 0);
  if(pte == 0 || (*pte & PTE_P) == 0)
    return;
  pa = PTE_ADDR(*pte);
  *pte = 0;
  flushtlb(pgdir);
  kfree(P2V(pa));
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)        // Lab 8: no page table (e.g. untouched lazy heap)
    return 0;
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
// Lab 8: copyout writes through the kernel mapping, which bypasses the
// user PTE's write bit, so it must first resolve the page like a write
// fault: allocate an untouched lazy page (Q4), break copy-on-write
// sharing (Q5) and refuse mprotect'ed pages (Q6).  Callers have already
// checked [va, va+len) against the process size, and either own pgdir
// (exec) or hold ptable.lock (clone).
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    if(uvmfault(pgdir, va0, 0, USERTOP, 1) < 0)
      return -1;
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

