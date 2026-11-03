// pagereplace.c — Lab 10 Q4: Page Replacement Algorithms (FIFO, LRU, Optimal)
//
// Simulates three page replacement algorithms in user space:
//   - FIFO  (First-In First-Out)
//   - LRU   (Least Recently Used)
//   - Optimal (clairvoyant — uses future knowledge)
//
// Reference string (20 references):
//   7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1
//
// The program runs each algorithm with 3, 4, and 5 frames and prints:
//   - Per-reference: current frame contents + HIT or FAULT
//   - Summary: total faults, total hits, hit ratio, fault ratio
//   - Comparison table across all frame counts

#include "types.h"
#include "stat.h"
#include "user.h"

// -------------------------------------------------------------------------
// Reference string
// -------------------------------------------------------------------------
static int refs[] = { 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1 };
#define NREFS  20
#define MAXFRAMES 5

// -------------------------------------------------------------------------
// Helper: print the current frame contents
// -------------------------------------------------------------------------
static void
print_frames(int *frames, int nframes)
{
  int i;
  printf(1, "[ ");
  for(i = 0; i < nframes; i++){
    if(frames[i] == -1)
      printf(1, "- ");
    else
      printf(1, "%d ", frames[i]);
  }
  printf(1, "]");
}

// -------------------------------------------------------------------------
// Helper: check if page p is in frames[]
// Returns index if found, -1 otherwise.
// -------------------------------------------------------------------------
static int
in_frames(int *frames, int nframes, int p)
{
  int i;
  for(i = 0; i < nframes; i++)
    if(frames[i] == p)
      return i;
  return -1;
}

// -------------------------------------------------------------------------
// FIFO Page Replacement
// Uses a circular queue.  The oldest loaded page is at position 'head' in
// the queue; on a fault the head slot is replaced and head advances.
// -------------------------------------------------------------------------
static void
run_fifo(int nframes, int *out_faults, int *out_hits)
{
  int frames[MAXFRAMES];
  int i, idx, head, faults, hits;

  for(i = 0; i < nframes; i++) frames[i] = -1;
  head   = 0;
  faults = 0;
  hits   = 0;

  printf(1, "\n--- FIFO  (frames = %d) ---\n", nframes);
  printf(1, "  Ref  Frames           Result\n");
  printf(1, "  ---  ---------------  ------\n");

  for(i = 0; i < NREFS; i++){
    int page = refs[i];
    idx = in_frames(frames, nframes, page);

    printf(1, "   %d   ", page);

    if(idx >= 0){
      // HIT
      hits++;
      print_frames(frames, nframes);
      printf(1, "   HIT\n");
    } else {
      // FAULT: replace the frame at head
      faults++;
      frames[head] = page;
      head = (head + 1) % nframes;
      print_frames(frames, nframes);
      printf(1, "   FAULT\n");
    }
  }

  *out_faults = faults;
  *out_hits   = hits;
}

// -------------------------------------------------------------------------
// LRU Page Replacement
// Tracks the time each frame was last used.  On a fault, evicts the frame
// with the smallest (oldest) last-used timestamp.
// -------------------------------------------------------------------------
static void
run_lru(int nframes, int *out_faults, int *out_hits)
{
  int frames[MAXFRAMES];
  int last_used[MAXFRAMES];   // logical timestamp of last access
  int i, idx, faults, hits, evict, min_time, filled;

  for(i = 0; i < nframes; i++){ frames[i] = -1; last_used[i] = 0; }
  faults = 0;
  hits   = 0;
  filled = 0;

  printf(1, "\n--- LRU   (frames = %d) ---\n", nframes);
  printf(1, "  Ref  Frames           Result\n");
  printf(1, "  ---  ---------------  ------\n");

  for(i = 0; i < NREFS; i++){
    int page = refs[i];
    idx = in_frames(frames, nframes, page);

    printf(1, "   %d   ", page);

    if(idx >= 0){
      // HIT: update last-used time
      hits++;
      last_used[idx] = i + 1;
      print_frames(frames, nframes);
      printf(1, "   HIT\n");
    } else {
      // FAULT
      faults++;
      if(filled < nframes){
        // Still have empty slots
        frames[filled]    = page;
        last_used[filled] = i + 1;
        filled++;
      } else {
        // Find frame with the smallest last_used (LRU victim)
        evict    = 0;
        min_time = last_used[0];
        int k;
        for(k = 1; k < nframes; k++){
          if(last_used[k] < min_time){
            min_time = last_used[k];
            evict    = k;
          }
        }
        frames[evict]    = page;
        last_used[evict] = i + 1;
      }
      print_frames(frames, nframes);
      printf(1, "   FAULT\n");
    }
  }

  *out_faults = faults;
  *out_hits   = hits;
}

// -------------------------------------------------------------------------
// Optimal Page Replacement
// On a fault, evicts the page whose next use is furthest in the future.
// If a page in a frame is never referenced again, it is the best victim.
// -------------------------------------------------------------------------

// Returns the index of the next occurrence of page p starting from pos,
// or NREFS (infinity) if p does not appear again.
static int
next_use(int p, int pos)
{
  int i;
  for(i = pos; i < NREFS; i++)
    if(refs[i] == p)
      return i;
  return NREFS;   // never used again — ideal victim
}

static void
run_optimal(int nframes, int *out_faults, int *out_hits)
{
  int frames[MAXFRAMES];
  int i, idx, faults, hits, filled;

  for(i = 0; i < nframes; i++) frames[i] = -1;
  faults = 0;
  hits   = 0;
  filled = 0;

  printf(1, "\n--- OPT   (frames = %d) ---\n", nframes);
  printf(1, "  Ref  Frames           Result\n");
  printf(1, "  ---  ---------------  ------\n");

  for(i = 0; i < NREFS; i++){
    int page = refs[i];
    idx = in_frames(frames, nframes, page);

    printf(1, "   %d   ", page);

    if(idx >= 0){
      // HIT
      hits++;
      print_frames(frames, nframes);
      printf(1, "   HIT\n");
    } else {
      // FAULT
      faults++;
      if(filled < nframes){
        // Empty slot available
        frames[filled++] = page;
      } else {
        // Evict the page whose next use is furthest away
        int evict = 0, max_next = -1, k, nu;
        for(k = 0; k < nframes; k++){
          nu = next_use(frames[k], i + 1);
          if(nu > max_next){
            max_next = nu;
            evict    = k;
          }
        }
        frames[evict] = page;
      }
      print_frames(frames, nframes);
      printf(1, "   FAULT\n");
    }
  }

  *out_faults = faults;
  *out_hits   = hits;
}

// -------------------------------------------------------------------------
// Print a comparison table row
// -------------------------------------------------------------------------
static void
print_row(const char *alg, int nf, int faults, int hits)
{
  // Print integer-only ratios as "N/20" to avoid floating-point
  printf(1, "  %-8s  %d      %2d         %2d       %2d/%d      %2d/%d\n",
         alg, nf, faults, hits, hits, NREFS, faults, NREFS);
}

// -------------------------------------------------------------------------
// main
// -------------------------------------------------------------------------
int
main(void)
{
  // Fault and hit counts for each algorithm × frame count combination
  int fifo_f[3], fifo_h[3];
  int lru_f[3],  lru_h[3];
  int opt_f[3],  opt_h[3];
  int fc, frame_counts[3];
  int i;

  frame_counts[0] = 3;
  frame_counts[1] = 4;
  frame_counts[2] = 5;

  printf(1, "========================================================\n");
  printf(1, " Lab 10 Q4 — Page Replacement Algorithm Simulation\n");
  printf(1, "========================================================\n");
  printf(1, " Reference string (%d refs): ", NREFS);
  for(i = 0; i < NREFS; i++)
    printf(1, "%d ", refs[i]);
  printf(1, "\n========================================================\n");

  // ---- Run all algorithms for each frame count ----
  for(fc = 0; fc < 3; fc++){
    int nf = frame_counts[fc];
    printf(1, "\n\n############################################\n");
    printf(1, "  Frame count = %d\n", nf);
    printf(1, "############################################\n");

    run_fifo(nf, &fifo_f[fc], &fifo_h[fc]);
    run_lru (nf, &lru_f[fc],  &lru_h[fc]);
    run_optimal(nf, &opt_f[fc],  &opt_h[fc]);

    printf(1, "\n  Summary for %d frames:\n", nf);
    printf(1, "    FIFO : faults=%d  hits=%d\n", fifo_f[fc], fifo_h[fc]);
    printf(1, "    LRU  : faults=%d  hits=%d\n", lru_f[fc],  lru_h[fc]);
    printf(1, "    OPT  : faults=%d  hits=%d\n", opt_f[fc],  opt_h[fc]);
  }

  // ---- Comparison table ----
  printf(1, "\n\n========================================================\n");
  printf(1, " Comparison Table (reference string length = %d)\n", NREFS);
  printf(1, "========================================================\n");
  printf(1, "  Algorithm Frames  Faults  Hits  Hit ratio  Fault ratio\n");
  printf(1, "  --------- ------  ------  ----  ---------  -----------\n");
  for(fc = 0; fc < 3; fc++){
    print_row("FIFO", frame_counts[fc], fifo_f[fc], fifo_h[fc]);
    print_row("LRU",  frame_counts[fc], lru_f[fc],  lru_h[fc]);
    print_row("OPT",  frame_counts[fc], opt_f[fc],  opt_h[fc]);
    if(fc < 2) printf(1, "  ---------  -  ------  ----  ---------  -----------\n");
  }

  printf(1, "\n--------------------------------------------------------\n");
  printf(1, " Observations:\n");
  printf(1, "  - OPT always achieves the minimum possible page faults.\n");
  printf(1, "  - LRU closely approximates OPT in practice.\n");
  printf(1, "  - FIFO can suffer from Belady's anomaly (more frames,\n");
  printf(1, "    more faults) though it does not appear for this string.\n");
  printf(1, "  - All algorithms improve as frame count increases.\n");
  printf(1, "========================================================\n");

  exit();
}
