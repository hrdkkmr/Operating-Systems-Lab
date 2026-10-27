#include "types.h"
#include "stat.h"
#include "user.h"

#define RESOURCE 0
#define RC_MUTEX 1
#define TURNSTILE 2
#define NUM_READERS 3
#define NUM_WRITERS 2
#define READER_LOOPS 3
#define WRITER_LOOPS 5

struct shared_data {
  volatile int data;
  volatile int read_count;
};

static void
delay(void)
{
  volatile int i;
  for(i = 0; i < 120000; i++)
    ;
}

static void
reader(struct shared_data *s, int id)
{
  int i;

  for(i = 0; i < READER_LOOPS; i++){
    // Pass through turnstile so a waiting writer blocks later readers.
    sem_wait(TURNSTILE);
    sem_signal(TURNSTILE);

    sem_wait(RC_MUTEX);
    s->read_count++;
    if(s->read_count == 1)
      sem_wait(RESOURCE);

    printf(1, "Reader %d ENTER, data = %d, readers = %d\n",
           id, s->data, s->read_count);

    sem_signal(RC_MUTEX);

    delay();

    sem_wait(RC_MUTEX);
    printf(1, "Reader %d EXIT\n", id);
    s->read_count--;

    if(s->read_count == 0)
      sem_signal(RESOURCE);

    sem_signal(RC_MUTEX);
    delay();
  }
}

static void
writer(struct shared_data *s, int id)
{
  int i;

  for(i = 0; i < WRITER_LOOPS; i++){
    sem_wait(TURNSTILE);
    sem_wait(RESOURCE);

    s->data++;
    printf(1, "Writer %d ENTER, data = %d\n", id, s->data);
    delay();
    printf(1, "Writer %d EXIT\n", id);

    sem_signal(RESOURCE);
    sem_signal(TURNSTILE);
    delay();
  }
}

int
main(void)
{
  struct shared_data *s;
  int i, pid, children;

  s = (struct shared_data*)shm_get();
  if(s == 0){
    printf(1, "shm_get failed\n");
    exit();
  }

  s->data = 0;
  s->read_count = 0;

  sem_init(RESOURCE, 1);
  sem_init(RC_MUTEX, 1);
  sem_init(TURNSTILE, 1);

  children = 0;

  for(i = 0; i < NUM_READERS; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      reader(s, i + 1);
      exit();
    }
    children++;
  }

  for(i = 0; i < NUM_WRITERS; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "fork failed\n");
      exit();
    }
    if(pid == 0){
      writer(s, i + 1);
      exit();
    }
    children++;
  }

  for(i = 0; i < children; i++)
    wait();

  printf(1, "Readers-Writers completed successfully\n");
  printf(1, "Final shared_data = %d\n", s->data);
  exit();
}
