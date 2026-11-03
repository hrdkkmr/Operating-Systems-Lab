// Lab 9 Q3: FIFO and LRU page replacement on a reference string.
//
// usage: pagerepl                       3 frames, the assignment's string
//        pagerepl NFRAMES PAGE PAGE ...  your own frame count and string
//
// For the chosen frame count it prints, for each algorithm, the frames
// after every reference and whether it was a hit or a fault.  Then it
// runs both algorithms with 3, 4 and 5 frames on the same string and
// prints a table of page faults and hit ratios.
#include "types.h"
#include "user.h"

#define MAXFRAMES 16
#define MAXREFS   128
#define FIFO 0
#define LRU  1

static char *algname[] = { "FIFO", "LRU" };

// The reference string from the assignment, task (b).
static int defrefs[] = { 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1 };

// Print v right-aligned in a field of width w (xv6 printf has no widths).
static void
printpad(int v, int w)
{
  int digits = 1, t = v;

  while(t >= 10){
    t /= 10;
    digits++;
  }
  while(digits++ < w)
    printf(1, " ");
  printf(1, "%d", v);
}

// Print hits/n as a percentage with two decimals, e.g. " 25.00%".
static void
printratio(int hits, int n)
{
  int r = (hits * 10000 + n / 2) / n;    // hundredths of a percent, rounded

  printpad(r / 100, 3);
  printf(1, ".%d%d%%", (r / 10) % 10, r % 10);
}

// Simulate one algorithm with nf frames on ref[0..n-1] and return the
// number of page faults.  With verbose != 0, print one line per reference.
//   FIFO: the victim is the page that has been in memory the longest;
//         `next` points at its slot and advances circularly.
//   LRU:  the victim is the page whose last use is the oldest;
//         lastuse[] holds the time of each frame's most recent reference.
int
simulate(int alg, int nf, int *ref, int n, int verbose)
{
  int frame[MAXFRAMES], lastuse[MAXFRAMES];
  int used = 0, next = 0, faults = 0;
  int t, i, slot, victim, hit;

  if(verbose){
    printf(1, "\n%s with %d frames\n", algname[alg], nf);
    printf(1, " ref   frames");
    for(i = 1; i < nf; i++)
      printf(1, "   ");
    printf(1, "  result\n");
  }
  for(t = 0; t < n; t++){
    slot = -1;
    for(i = 0; i < used; i++)
      if(frame[i] == ref[t])
        slot = i;
    hit = (slot >= 0);
    victim = -1;                        // -1: no page was evicted
    if(!hit){                           // page fault
      faults++;
      if(used < nf)                     // a free frame is left
        slot = used++;
      else {
        if(alg == FIFO){
          slot = next;
          next = (next + 1) % nf;
        } else {
          slot = 0;
          for(i = 1; i < nf; i++)
            if(lastuse[i] < lastuse[slot])
              slot = i;
        }
        victim = frame[slot];
      }
      frame[slot] = ref[t];
    }
    lastuse[slot] = t;

    if(verbose){
      printpad(ref[t], 4);
      printf(1, "   ");
      for(i = 0; i < nf; i++){
        if(i < used)
          printpad(frame[i], 2);
        else
          printf(1, " -");
        printf(1, " ");
      }
      if(hit)
        printf(1, "  hit\n");
      else if(victim >= 0)
        printf(1, "  FAULT (replaced %d)\n", victim);
      else
        printf(1, "  FAULT (free frame)\n");
    }
  }
  if(verbose)
    printf(1, "%s, %d frames: %d page faults, %d hits out of %d references\n",
           algname[alg], nf, faults, n - faults, n);
  return faults;
}

int
main(int argc, char *argv[])
{
  int refbuf[MAXREFS], *ref, n, nf, i, f, alg;

  if(argc == 1){
    ref = defrefs;
    n = sizeof(defrefs) / sizeof(defrefs[0]);
    nf = 3;
  } else {
    if(argc < 3){
      printf(2, "usage: pagerepl [NFRAMES PAGE PAGE ...]\n");
      exit();
    }
    nf = atoi(argv[1]);
    n = argc - 2;
    if(nf < 1 || nf > MAXFRAMES || n > MAXREFS){
      printf(2, "pagerepl: need 1..%d frames and at most %d references\n",
             MAXFRAMES, MAXREFS);
      exit();
    }
    for(i = 0; i < n; i++)
      refbuf[i] = atoi(argv[i + 2]);
    ref = refbuf;
  }

  printf(1, "pagerepl: %d references:", n);
  for(i = 0; i < n; i++)
    printf(1, " %d", ref[i]);
  printf(1, "\n");

  for(alg = FIFO; alg <= LRU; alg++)
    simulate(alg, nf, ref, n, 1);

  printf(1, "\nSummary (%d references)\n", n);
  printf(1, "frames | FIFO faults  FIFO hit ratio | LRU faults  LRU hit ratio\n");
  printf(1, "-------+-----------------------------+---------------------------\n");
  for(f = 3; f <= 5; f++){
    int ff = simulate(FIFO, f, ref, n, 0);
    int lf = simulate(LRU, f, ref, n, 0);
    printpad(f, 6);
    printf(1, " | ");
    printpad(ff, 11);
    printf(1, "        ");
    printratio(n - ff, n);
    printf(1, " | ");
    printpad(lf, 10);
    printf(1, "        ");
    printratio(n - lf, n);
    printf(1, "\n");
  }
  exit();
}
