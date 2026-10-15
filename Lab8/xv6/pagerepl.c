// Lab 8 Q8 (inferred extension, see submission/FINAL_STATUS.md):
// page-replacement simulation: FIFO, LRU and Optimal.
//
// This is a user-space simulation; it does not change how the xv6
// kernel manages physical memory.
//
//   pagerepl                        built-in experiments (below)
//   pagerepl NFRAMES P1 P2 ... Pn   one custom reference string
//                                   (the xv6 shell accepts at most 9 words
//                                   per command, so n <= 7 from sh)
//
// Built-in experiments (all three algorithms always get the same
// reference string and the same number of frames):
//   A: 7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1 with 3 frames
//      (full step-by-step trace) and with 4 frames
//   B: 1 2 3 4 1 2 5 1 2 3 4 5 with 3 and 4 frames (Belady's anomaly)

#include "types.h"
#include "stat.h"
#include "user.h"

#define MAXREF    64
#define MAXFRAMES 16
#define NELEM(x)  (sizeof(x) / sizeof((x)[0]))

enum { FIFO, LRU, OPT, NALG };
static char *algname[NALG] = { "FIFO", "LRU", "Optimal" };

// Simulate one algorithm.  verbose prints one line per reference:
// the page, the frame contents after the reference, hit/fault and the
// evicted page.  Returns the number of page faults.
static int
simulate(int alg, int *ref, int n, int nframes, int verbose)
{
  int frame[MAXFRAMES], loaded[MAXFRAMES], used[MAXFRAMES];
  int i, j, k, page, slot, victim, faults, next, far, hit;

  for(j = 0; j < nframes; j++)
    frame[j] = -1;
  faults = 0;
  if(verbose)
    printf(1, "\n%s, %d frames:\n  step page | frames | result\n",
           algname[alg], nframes);
  for(i = 0; i < n; i++){
    page = ref[i];
    slot = -1;
    for(j = 0; j < nframes; j++)
      if(frame[j] == page)
        slot = j;
    victim = -1;
    hit = slot >= 0;
    if(hit){
      used[slot] = i;                  // hit: only LRU cares
    } else {
      faults++;
      for(j = 0; j < nframes; j++)     // a free frame first
        if(frame[j] == -1){
          slot = j;
          break;
        }
      if(slot < 0){
        if(alg == FIFO){
          // evict the page that was loaded first
          slot = 0;
          for(j = 1; j < nframes; j++)
            if(loaded[j] < loaded[slot])
              slot = j;
        } else if(alg == LRU){
          // evict the page whose last use is oldest
          slot = 0;
          for(j = 1; j < nframes; j++)
            if(used[j] < used[slot])
              slot = j;
        } else {
          // Optimal: look ahead in the real future reference string and
          // evict the page used farthest in the future (or never again).
          // Ties (several pages never used again) go to the lowest frame.
          far = -1;
          for(j = 0; j < nframes; j++){
            next = n;                  // "never used again"
            for(k = i + 1; k < n; k++)
              if(ref[k] == frame[j]){
                next = k;
                break;
              }
            if(next > far){
              far = next;
              slot = j;
            }
          }
        }
        victim = frame[slot];
      }
      frame[slot] = page;
      loaded[slot] = i;
      used[slot] = i;
    }
    if(verbose){
      printf(1, "  %d%s   %d   | ", i + 1, i + 1 < 10 ? " " : "", page);
      for(j = 0; j < nframes; j++){
        if(frame[j] < 0)
          printf(1, " .");
        else
          printf(1, " %d", frame[j]);
      }
      printf(1, hit ? "   | hit" : "   | FAULT");
      if(victim >= 0)
        printf(1, " (evict %d)", victim);
      printf(1, "\n");
    }
  }
  return faults;
}

static void
printref(int *ref, int n)
{
  int i;

  for(i = 0; i < n; i++)
    printf(1, "%d%s", ref[i], i + 1 < n ? " " : "");
}

// Run all three algorithms on the same input and print a summary row.
static void
experiment(char *name, int *ref, int n, int nframes, int verbose)
{
  int f[NALG], a;

  for(a = 0; a < NALG; a++)
    f[a] = simulate(a, ref, n, nframes, verbose);
  printf(1, "| %s | %d | ", name, nframes);
  printref(ref, n);
  printf(1, " | %d | %d | %d |\n", f[FIFO], f[LRU], f[OPT]);
}

static int refA[] = { 7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1 };
static int refB[] = { 1,2,3,4,1,2,5,1,2,3,4,5 };

int
main(int argc, char *argv[])
{
  int ref[MAXREF], n, nframes, i;

  printf(1, "\n=== pagerepl: Q8 FIFO / LRU / Optimal page replacement ===\n");
  if(argc > 1){
    nframes = atoi(argv[1]);
    n = argc - 2;
    if(nframes < 1 || nframes > MAXFRAMES || n < 1 || n > MAXREF){
      printf(2, "usage: pagerepl NFRAMES(1-%d) P1 ... Pn (1 <= n <= %d, "
             "pages >= 0)\n", MAXFRAMES, MAXREF);
      exit();
    }
    for(i = 0; i < n; i++){
      if(argv[i + 2][0] < '0' || argv[i + 2][0] > '9'){
        printf(2, "pagerepl: bad page number %s\n", argv[i + 2]);
        exit();
      }
      ref[i] = atoi(argv[i + 2]);
    }
    printf(1, "\n| String | Frames | Reference string | FIFO | LRU | Optimal |\n");
    experiment("custom", ref, n, nframes, 1);
    exit();
  }

  printf(1, "\nStep-by-step trace, string A with 3 frames:");
  for(i = 0; i < NALG; i++)
    simulate(i, refA, NELEM(refA), 3, 1);

  printf(1, "\nPage faults (same reference string and frame count for all "
         "three algorithms):\n\n");
  printf(1, "| String | Frames | Reference string | FIFO | LRU | Optimal |\n");
  printf(1, "|---|---:|---|---:|---:|---:|\n");
  experiment("A", refA, NELEM(refA), 3, 0);
  experiment("A", refA, NELEM(refA), 4, 0);
  experiment("B", refB, NELEM(refB), 3, 0);
  experiment("B", refB, NELEM(refB), 4, 0);
  exit();
}
