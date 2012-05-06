#include <pthread.h>
#include <stdio.h>

#define MAX_N_THREADS (1024)

typedef void *(*ThreadFuncType)(void *);

ThreadFuncType thr_func;

void *worker(void *arg) {
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t child;
  thr_func = worker;
  pthread_create(&child, NULL, thr_func, NULL);
  pthread_join(child, NULL);

  return 0;
}
