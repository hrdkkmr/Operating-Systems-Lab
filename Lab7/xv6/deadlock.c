#include "types.h"
#include "stat.h"
#include "user.h"

#define N 3
#define M 3

int is_safe(int avail[M], int max[N][M], int alloc[N][M]) {
  int work[M], finish[N] = {0};
  for(int i = 0; i < M; i++) work[i] = avail[i];

  int count = 0;
  while(count < N){
    int found = 0;
    for(int p = 0; p < N; p++){
      if(!finish[p]){
        int j;
        for(j = 0; j < M; j++){
          if(max[p][j] - alloc[p][j] > work[j])
            break;
        }
        if(j == M){
          for(int k = 0; k < M; k++) work[k] += alloc[p][k];
          finish[p] = 1;
          found = 1;
          count++;
        }
      }
    }
    if(!found) break;
  }
  return count == N;
}

int main(void) {
  int alloc[N][M] = {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}};
  int max[N][M]   = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}};
  int avail[M]    = {3, 3, 2};

  printf(1, "Safe state: %s\n", is_safe(avail, max, alloc) ? "YES" : "NO");
  exit();
}