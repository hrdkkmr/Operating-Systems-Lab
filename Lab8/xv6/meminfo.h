// Lab 8 Q2: memory statistics returned by getmeminfo().
// Shared by the kernel (sysproc.c) and user programs.  All counts are in
// 4096-byte pages.
struct meminfo {
  int total_pages;     // physical pages managed by the allocator (kalloc.c)
  int free_pages;      // pages currently on the allocator's free list
  int proc_pages;      // caller's user address space: sz / PGSIZE
                       // (logical size; includes untouched lazy heap)
  int pt_pages;        // caller's page directory + all its page-table
                       // pages (user and kernel half)
  // Extra detail, not required by the assignment text:
  int pt_user_pages;   // page-table pages covering va < KERNBASE
  int resident_pages;  // present PTE_U pages below KERNBASE
                       // (physically backed user pages, incl. shared ones)
  int shared_pages;    // resident pages whose frame has refcount > 1
                       // (copy-on-write or shared-memory frames)
};
