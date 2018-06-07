#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>

#define THREADS 't'
#define ITERATIONS 'i'
#define SYNC 's'
#define YIELD 'y'
#define MUTEX 'm'
#define SPIN_LOCK 's'
#define COMPARE_AND_SWAP 'c'

int NUM_THREADS;
int NUM_ITER;
int i;
int j;
char* sync_ver;
int sync_flag;
int opt_yield;

int spin_lock = 0;

static long long counter;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void getParameters(int argc, char* argv[]);
void add(long long *pointer, long long value);
void addWrap(void);
void printCSV(long long ns);
void systemCallErr(char syscall[]);
void* threadHandler(void* ver);

int main(int argc, char* argv[]) {
  
  NUM_THREADS = 1;
  NUM_ITER = 1;
  sync_flag = 0;
  opt_yield = 0;

  getParameters(argc, argv);
  struct timespec start, end;

  counter = 0;

  if (sync_flag && *sync_ver == 'm')
    pthread_mutex_init(&mutex, NULL);
  
  //note the start time
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    systemCallErr("clock_gettime");

  pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * NUM_THREADS);
  if (threads == NULL)
    systemCallErr("malloc");

  for (i = 0; i < NUM_THREADS; i++)
    {
      //create thread
      if (pthread_create(&threads[i], NULL, threadHandler, (void *) sync_ver) != 0)
	systemCallErr("pthread_create");
    }

  void* status;
  for (i = 0; i < NUM_THREADS; i++)
    {
      //join threads
      if (pthread_join(threads[i], &status) != 0)
	systemCallErr("pthread_join");
    }
  free(threads);

  //note the end time
  if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
    systemCallErr("clock_gettime");

  long long elapsed_ns = end.tv_nsec - start.tv_nsec;
    
  printCSV(elapsed_ns);
  pthread_exit(NULL);
  exit(0);
}

void* threadHandler(void * ver)
{
  int i = 0;
  if (sync_flag)
    {    
      char* sync = (char*) ver;
      for (i = 0; i < NUM_ITER; i++)
	{
	  switch (*sync)
	    {
	    case MUTEX:
	      //mutex lock
	      pthread_mutex_lock(&mutex);
	      add(&counter, 1);
	      pthread_mutex_unlock(&mutex);
	      break;
	    case SPIN_LOCK:
	      //spin lock
	      while (__sync_lock_test_and_set(&spin_lock, 1) == 1)
		;
	      add(&counter, 1);
	      __sync_lock_release(&spin_lock);
	      break;
	    case COMPARE_AND_SWAP:
	      add(&counter, 1);
	      break;
	    }
	}
      for (i = 0; i < NUM_ITER; i++)
	{
	  switch (*sync)
	    {
	    case MUTEX:
	      //mutex lock
	      pthread_mutex_lock(&mutex);
	      add(&counter, -1);
	      pthread_mutex_unlock(&mutex);
	      break;
	    case SPIN_LOCK:
	      //spin lock
	      while (__sync_lock_test_and_set(&spin_lock, 1) == 1)
		;
	      add(&counter, -1);
	      __sync_lock_release(&spin_lock);
	      break;
	    case COMPARE_AND_SWAP:
	      add(&counter, -1);
	      break;
	    }
	}
    }
  else
    {
      for (i = 0; i < NUM_ITER; i++)
	add(&counter, 1);
      for (i = 0; i < NUM_ITER; i++)
	add(&counter, -1);
    }
  pthread_exit(NULL);
}


void add(long long *pointer, long long value) 
{
  long long sum;
  if (sync_flag && *sync_ver == COMPARE_AND_SWAP)
    {
      long long prev;
      do
	{
	  prev = *pointer;
	  sum = prev + value;
	  if (opt_yield)
	    sched_yield();
	} while (__sync_val_compare_and_swap(pointer, prev, sum) != prev);
      return;
    }
  else
    {
      sum = *pointer + value;
      if (opt_yield)
	sched_yield();
      *pointer = sum;
    }
}

void getParameters(int argc, char* argv[])
{
  static struct option long_options[] = {
    {"threads", required_argument, 0, THREADS},
    {"iterations", required_argument, 0, ITERATIONS},
    {"sync", required_argument, 0, SYNC},
    {"yield", no_argument, 0, YIELD},
    {0, 0, 0, 0}
  }; 

  int opt = 0;
  while ((opt = getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch (opt)
	{
	case THREADS:
	  NUM_THREADS = atoi(optarg);
	  if (NUM_THREADS < 0)
	    {
	      fprintf(stderr, "Number of threads should be positive!");
	      exit(1);
	    }
	  break;
	case ITERATIONS:
	  NUM_ITER = atoi(optarg);
	  if (NUM_ITER < 0)
	    {
	      fprintf(stderr, "Number of iterations should be positive!");
	      exit(1);
	    }
	  break;
	case SYNC:
	  sync_ver = optarg;
	  sync_flag = 1;
	  break;
	case YIELD:
	  opt_yield = 1;
	  break;
	default:
	  fprintf(stderr, "usage: ./lab2a_add --threads=VAL --iterations=VAL [--yield] [--sync=[smc]]\n");
	  exit(1);
	  break;
	}
    }
}

void printCSV(long long ns)
{
  printf("add-");
  if (opt_yield)
    printf("yield-");
  if (sync_flag)
    {
      //switch between different types of flags
      switch (*sync_ver)
	{
	case MUTEX:
	  printf("m,");
	  break;
	case SPIN_LOCK:
	  printf("s,");
	  break;
	case COMPARE_AND_SWAP:
	  printf("c,");
	default:
	  break;
	}
    }
  else
    {
      printf("none,");
    }
  long ops = NUM_THREADS * NUM_ITER * 2;
  long long avg_time_per_op = ns/ops;
  printf("%d%c%d%c", NUM_THREADS, ',', NUM_ITER, ',');
  printf("%ld%c%lld%c", ops, ',', ns, ',');
  printf("%lld%c%lld\n", avg_time_per_op, ',', counter);
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}

