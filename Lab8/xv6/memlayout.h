// Memory layout

#define EXTMEM  0x100000            // Start of extended memory
#define PHYSTOP 0xE000000           // Top physical memory
#define DEVSPACE 0xFE000000         // Other devices are at high addresses

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000         // First kernel virtual address
#define SHMVA (KERNBASE - PGSIZE)   // Fixed user VA for assignment shared memory
#define KERNLINK (KERNBASE+EXTMEM)  // Address where kernel is linked

// Lab 8 Q7: keyed shared-memory regions, one page per key, at fixed user
// addresses just below the Lab 5 page:  SHMREGBASE + key*PGSIZE.
#define NSHMREG    8
#define SHMREGBASE (SHMVA - NSHMREG*PGSIZE)
// Lab 8: highest address the heap (proc->sz) may grow to.  Everything
// from USERTOP up to KERNBASE is reserved for the shared-memory pages,
// so a (lazy) heap can never run into them.
#define USERTOP    SHMREGBASE

#define V2P(a) (((uint) (a)) - KERNBASE)
#define P2V(a) ((void *)(((char *) (a)) + KERNBASE))

#define V2P_WO(x) ((x) - KERNBASE)    // same as V2P, but without casts
#define P2V_WO(x) ((x) + KERNBASE)    // same as P2V, but without casts
