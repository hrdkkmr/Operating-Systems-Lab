# OS Lab Assignment 5 — Process Synchronization (xv6 x86)

This repository contains an x86 xv6 implementation of the four synchronization exercises.

## Programs

- `peterson` — Peterson's algorithm for two processes using one shared page.
- `prodcons` — bounded circular-buffer producer/consumer using counting semaphores and a mutex.
- `readwrite` — readers-writers with reader concurrency and a turnstile to prevent writer starvation.
- `dining` — five dining philosophers using one binary semaphore per fork and resource ordering.

## Kernel changes

- `SHMVA` reserves one fixed user virtual address for the synchronization shared page.
- `struct proc` stores the physical address of its shared page mapping.
- `fork()` maps the same physical shared page into the child and maintains a reference count.
- `wait()` releases a child's shared-memory mapping before `freevm()`.
- `semaphore.c` implements lightweight counting/binary semaphores with xv6 `sleep()`/`wakeup()`.
- New system calls:
  - `shm_get()`
  - `sem_init(id, value)`
  - `sem_wait(id)`
  - `sem_signal(id)`

The kernel semaphore is only used for Q2–Q4. Q1 mutual exclusion itself is implemented by Peterson's shared `flag[]` and `turn` variables.

## Build

From the repository root:

```bash
make clean
make
make fs.img
```

Run xv6 with QEMU:

```bash
make qemu
```

At the xv6 shell:

```text
$ peterson
$ prodcons
$ readwrite
$ dining
```

For producer-consumer, an optional buffer capacity can be supplied:

```text
$ prodcons 3
```

The program accepts capacities from 1 to 5.

## Verification

### Q1
Both processes execute 10 critical sections, so the final counter must be exactly 20.

### Q2
Items 1 through 20 must be consumed in FIFO order with no duplicates or losses.

### Q3
Multiple readers may enter together; writers must be exclusive. The turnstile prevents new readers from continually bypassing a waiting writer.

### Q4
Each philosopher completes 5 cycles. Forks are always acquired in increasing fork-number order, removing circular wait and preventing deadlock.

## Output logs

The `output_logs/` directory contains instructions for recording real QEMU output. Runtime logs should be captured from the tested xv6 build; no fabricated runtime results are included.
