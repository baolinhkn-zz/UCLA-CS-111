#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "SortedList.h"

#define THREADS 't'
#define ITERATIONS 'i'
#define YIELD 'y'
#define SYNC 's'
#define LISTS 'l'

#define INSERT 'i'
#define DELETE 'd'
#define LOOKUP 'l'
#define MUTEX 'm'
#define SPIN_LOCK 's'

long FAIL = 2;

int NUM_THREADS;
int NUM_ITER;

char* YIELD_OPTIONS;
int yield_flag;
int opt_yield;
int sync_flag;
char* sync_ver;

static pthread_mutex_t* mutexes;
int* spin_locks;

long long elapsed_lock_ns;

long long num_elements;
int num_lists;
SortedList_t** head;
SortedListElement_t** list_elements;

void getParameters(int argc, char* argv[]);
void processYieldOptions(void);
void systemCallErr(char syscall[]);
char* generateRandomKey(void);
SortedListElement_t** generateListElements(long long len);
void* threadHandler(void* list);
void printCSV(long long ns);
void handler();
long long timeDif(long long dif);

int main(int argc, char* argv[])
{
  //register segmentation fault handler
  signal(SIGSEGV, handler);

  NUM_THREADS=1;
  NUM_ITER=1;
  num_lists = 1;
  opt_yield = 0;
  yield_flag = 0;
  sync_flag = 0;
  getParameters(argc, argv);
  
  elapsed_lock_ns = 0;

  int i;  

  //initialize locks
  if (sync_flag && *sync_ver == 'm')
    {
      mutexes = (pthread_mutex_t*) malloc (sizeof(pthread_mutex_t) * num_lists);
      if (mutexes == NULL)
	systemCallErr("malloc");
      for (i = 0; i < num_lists; i++)
	pthread_mutex_init(&mutexes[i], NULL);
    }

  if (sync_flag && *sync_ver == 's')
    {
      spin_locks = (int*) malloc(sizeof(int) * num_lists);
      if (spin_locks == NULL)
	systemCallErr("malloc");
      for (i = 0; i < num_lists; i++)
	spin_locks[i] = 0;
	
    }

  num_elements = NUM_THREADS * NUM_ITER;
  list_elements = generateListElements(num_elements);

  //create sublists
  head = (SortedList_t**) malloc(sizeof(SortedList_t*) * num_lists);
  if (head == NULL)
    systemCallErr("malloc");
  for (i = 0; i < num_lists; i++)
    {
      head[i] = (SortedList_t*) malloc(sizeof(SortedList_t));
      head[i]->key = NULL;
      head[i]->next = head[i];
      head[i]->prev = head[i];
    }

  //start timer
  struct timespec start, end;
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    systemCallErr("clock_gettime");

  //start threads
  pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * NUM_THREADS);
  int* tIDs = (int*) malloc(sizeof(int) * NUM_THREADS);

  if (threads == NULL)
    systemCallErr("malloc");

  for (i = 0; i < NUM_THREADS; i++)
    {
      tIDs[i] = i;
      if (pthread_create(&threads[i], NULL, threadHandler, (void*) &tIDs[i]) != 0)
	systemCallErr("pthread_create");
    }
  
  void* status;
  for (i = 0; i < NUM_THREADS; i++)
    {
      if (pthread_join(threads[i], &status) != 0)
	systemCallErr("pthread_join");
    }
  
  //end timer
  if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
    systemCallErr("clock_gettime");

  free(threads);
  free(mutexes);
  free(head);
  free(spin_locks);

  long long elapsed_ns = end.tv_nsec - start.tv_nsec;

  elapsed_ns = timeDif(elapsed_ns);

  printCSV(elapsed_ns);
  pthread_exit(NULL);
  return 0;
}

void* threadHandler(void* threadID)
{
  int tID = *(int*)threadID;
  int i;


  for (i = tID; i < num_elements; i = i + NUM_THREADS)
    {
      //which sublist the element goes into
      int index = abs(( (int) *(list_elements[i]->key)) % num_lists);

      if (sync_flag && *sync_ver == 'm')
	{
	  struct timespec mutex_start, mutex_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_start) == -1)
	    systemCallErr("clock_gettime");
	  pthread_mutex_lock(&mutexes[index]);
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_end) == -1)
	    systemCallErr("clock_gettime");
	  long temp_time = mutex_end.tv_nsec - mutex_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}
      else  if (sync_flag && *sync_ver == 's')
	{
	  struct timespec spin_start, spin_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_start) == -1)
	    systemCallErr("clock_gettime");
      	  while(__sync_lock_test_and_set(&spin_locks[index], 1) == 1)
	    ;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_end) == -1)
	    systemCallErr("clock_gettime");
	  long long temp_time = spin_end.tv_nsec - spin_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}

      SortedList_insert(head[index], list_elements[i]);

      if (sync_flag && *sync_ver == 'm')
	pthread_mutex_unlock(&mutexes[index]);
      else if (sync_flag && *sync_ver == 's')
	__sync_lock_release(&spin_locks[index]);

    }

  long long length = 0;

  for (i = 0; i < num_lists; i++)
    {
      if (sync_flag && *sync_ver == 'm')
	{
	  struct timespec mutex_start, mutex_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_start) == -1)
	    systemCallErr("clock_gettime");
	  pthread_mutex_lock(&mutexes[i]);
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_end) == -1)
	    systemCallErr("clock_gettime");
	  long temp_time = mutex_end.tv_nsec - mutex_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}
      else if (sync_flag && *sync_ver == 's')
	{
	  struct timespec spin_start, spin_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_start) == -1)
	    systemCallErr("clock_gettime");
	  while(__sync_lock_test_and_set(&spin_locks[i], 1) == 1)
	    ;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_end) == -1)
	    systemCallErr("clock_gettime");
	  long long temp_time = spin_end.tv_nsec - spin_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}

      length = SortedList_length(head[i]);

      if (length == -1)
	{
	  fprintf(stderr, "Corrupted list!\n");
	  exit(FAIL);
	}

      if (sync_flag && *sync_ver == 'm')
	pthread_mutex_unlock(&mutexes[i]);
      else if (sync_flag && *sync_ver == 's')
	__sync_lock_release(&spin_locks[i]);
    }
  
  for (i = tID; i < num_elements; i = i + NUM_THREADS)
    {
      int index = abs(( (int) *(list_elements[i]->key)) % num_lists);
      if (sync_flag && *sync_ver == 'm')
	{
	  struct timespec mutex_start, mutex_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_start) == -1)
	    systemCallErr("clock_gettime");
	  pthread_mutex_lock(&mutexes[index]);
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_end) == -1)
	    systemCallErr("clock_gettime");
	  long temp_time = mutex_end.tv_nsec - mutex_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}
      else if (sync_flag && *sync_ver == 's')
	{
	  struct timespec spin_start, spin_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_start) == -1)
	    systemCallErr("clock_gettime");
	  while(__sync_lock_test_and_set(&spin_locks[index], 1) == 1)
	    ;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_end) == -1)
	    systemCallErr("clock_gettime");
	  long long temp_time = spin_end.tv_nsec - spin_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}

      SortedListElement_t* element = SortedList_lookup(head[index], list_elements[i]->key);
      if (element == NULL)
	{
	  fprintf(stderr, "Corrupted List! Returned a null element in lookup\n");
	  exit(FAIL);
	}
      if (SortedList_delete(element) == 1)
	{
	  fprintf(stderr, "Corrupted List! Error in element deletion\n");
	  exit(FAIL);
	}	    
      
      if (sync_flag && *sync_ver == 'm')
	pthread_mutex_unlock(&mutexes[index]);
      else if (sync_flag && *sync_ver == 's')
	__sync_lock_release(&spin_locks[index]);
    }


  length = 0;
  for (i = 0; i < num_lists; i++)
    {
      if (sync_flag && *sync_ver == 'm')
	{
	  struct timespec mutex_start, mutex_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_start) == -1)
	    systemCallErr("clock_gettime");
	  pthread_mutex_lock(&mutexes[i]);
	  if (clock_gettime(CLOCK_MONOTONIC, &mutex_end) == -1)
	    systemCallErr("clock_gettime");
	  long temp_time = mutex_end.tv_nsec - mutex_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}
      else if (sync_flag && *sync_ver == 's')
	{
	  struct timespec spin_start, spin_end;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_start) == -1)
	    systemCallErr("clock_gettime");
	  while(__sync_lock_test_and_set(&spin_locks[i], 1) == 1)
	    ;
	  if (clock_gettime(CLOCK_MONOTONIC, &spin_end) == -1)
	    systemCallErr("clock_gettime");
	  long long temp_time = spin_end.tv_nsec - spin_start.tv_nsec;
	  elapsed_lock_ns = elapsed_lock_ns + timeDif(temp_time);
	}


      length = SortedList_length(head[i]);

      if (length == -1)
	{
	  fprintf(stderr, "Corrupted List!\n");
	  exit(FAIL);
	}

      if (sync_flag && *sync_ver == 'm')
	pthread_mutex_unlock(&mutexes[i]);
      else if (sync_flag && *sync_ver == 's')
	__sync_lock_release(&spin_locks[i]);
    }
  pthread_exit(NULL);
}

SortedListElement_t** generateListElements(long long len)
{
  SortedListElement_t** list_elements = (SortedListElement_t**) malloc((sizeof(SortedListElement_t)) * len);

  if (list_elements == NULL)
    systemCallErr("malloc");
  
  long long i = 0;
  for (i = 0; i < len; i++)
    {
      list_elements[i] = (SortedListElement_t*) malloc(sizeof(SortedListElement_t));
      if (list_elements[i] == NULL)
	systemCallErr("malloc");
      list_elements[i]->key = generateRandomKey();
      list_elements[i]->next = NULL;
      list_elements[i]->prev = NULL;
    }
  return list_elements;
}

void printCSV(long long ns)
{
  printf("list-");
  if (yield_flag == 0)
    printf("none-");
  else
    printf("%s-", YIELD_OPTIONS);
  long long total_ops = NUM_THREADS * NUM_ITER * 3;
  long long avg_time = ns/total_ops;
  if (sync_flag)
    printf("%s,", sync_ver);
  else
    printf("none,");

  printf("%i,%i,%i,%lld,%lld,%lld", NUM_THREADS, NUM_ITER, num_lists, total_ops, ns, avg_time);
  long long avg_wait_for_lock = elapsed_lock_ns/NUM_THREADS;
  printf(",%lld\n", avg_wait_for_lock);
}

char* generateRandomKey(void)
{
  char* s = (char*) malloc(sizeof(char) * 20);
  if (s == NULL)
    systemCallErr("malloc");
  static char alphanum[] = 
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  int i;
  for (i = 0; i < 20; i++)
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  s[20] = 0;
  return s;
}

long long timeDif(long long dif)
{
  if (dif < 0)
    return dif+1000000000;
  return dif;
}

void getParameters(int argc, char* argv[])
{
  static struct option long_options[] = {
    {"threads", required_argument, 0, THREADS},
    {"iterations", required_argument, 0, ITERATIONS},
    {"yield", required_argument, 0, YIELD},
    {"sync", required_argument, 0, SYNC},
    {"lists", required_argument, 0, LISTS},
    {0, 0, 0, 0},
  };

  int opt = 0;
  while ((opt = getopt_long(argc, argv, "", long_options, 0)) != -1)
    {
      switch(opt)
	{
	case THREADS:
	  NUM_THREADS=atoi(optarg);
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
	case YIELD:
	  yield_flag = 1;
	  YIELD_OPTIONS=optarg;
	  processYieldOptions();
	  break;
	case SYNC:
	  sync_ver = optarg;
	  sync_flag = 1;
	  break;
	case LISTS:
	  num_lists = atoi(optarg);
	  break;
	default:
	  fprintf(stderr, "usage: ./lab2a_list --threads=VAL --iterations=VAL [--yield=[idl]] [--sync=[ms]]\n");
	  exit(1);
	  break;
	}
    }
}

void processYieldOptions(void)
{
  int len = strlen(YIELD_OPTIONS);
  int i = 0;
  for (i = 0; i < len; i++)
    {
      switch(YIELD_OPTIONS[i])
	{
	case INSERT:
	  opt_yield |= INSERT_YIELD;
	  break;
	case DELETE:
	  opt_yield |= DELETE_YIELD;
	  break;
	case LOOKUP:
	  opt_yield |= LOOKUP_YIELD;
	  break;
	default:
	  fprintf(stderr, "wrong parameter for yield options, usage should be --yield[idl]\n");
	  exit(1);
	  break;
	}
    }
}

void systemCallErr(char syscall[])
{
  fprintf(stderr, "Error with %s system call\n", syscall);
  fprintf(stderr, "%s\n", strerror(errno));
  exit(1);
}

void handler()
{
  fprintf(stderr, "Segmentation Fault occured\n");
  exit(FAIL);
}
