#include "types.h"
#include "stat.h"
#include "user.h"

#define BUFFER_SIZE 5
#define ITEMS 20
#define EMPTY 0
#define FULL 1
#define MUTEX 2

struct shared_buffer {
  int buffer[BUFFER_SIZE];
  int in;
  int out;
  int count;
};

static void
delay(void)
{
  volatile int i;
  for(i = 0; i < 150000; i++)
    ;
}

static void
producer(struct shared_buffer *b, int capacity)
{
  int item;

  for(item = 1; item <= ITEMS; item++){
    sem_wait(EMPTY);
    sem_wait(MUTEX);

    b->buffer[b->in] = item;
    b->in = (b->in + 1) % capacity;
    b->count++;

    printf(1, "Producer: produced %d (buffer count = %d)\n",
           item, b->count);

    sem_signal(MUTEX);
    sem_signal(FULL);
    delay();
  }
}

static void
consumer(struct shared_buffer *b, int capacity)
{
  int i, item;

  for(i = 0; i < ITEMS; i++){
    sem_wait(FULL);
    sem_wait(MUTEX);

    item = b->buffer[b->out];
    b->out = (b->out + 1) % capacity;
    b->count--;

    printf(1, "Consumer: consumed %d (buffer count = %d)\n",
           item, b->count);

    sem_signal(MUTEX);
    sem_signal(EMPTY);
    delay();
  }
}

int
main(int argc, char *argv[])
{
  struct shared_buffer *b;
  int capacity, pid;

  capacity = BUFFER_SIZE;
  if(argc > 1)
    capacity = atoi(argv[1]);

  if(capacity < 1 || capacity > BUFFER_SIZE){
    printf(1, "Buffer size must be between 1 and %d\n", BUFFER_SIZE);
    exit();
  }

  b = (struct shared_buffer*)shm_get();
  if(b == 0){
    printf(1, "shm_get failed\n");
    exit();
  }

  b->in = 0;
  b->out = 0;
  b->count = 0;

  sem_init(EMPTY, capacity);
  sem_init(FULL, 0);
  sem_init(MUTEX, 1);

  pid = fork();
  if(pid < 0){
    printf(1, "fork failed\n");
    exit();
  }

  if(pid == 0){
    consumer(b, capacity);
    exit();
  }

  producer(b, capacity);
  wait();
  printf(1, "Producer-Consumer completed successfully\n");
  exit();
}
