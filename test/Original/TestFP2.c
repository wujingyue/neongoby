#include <pthread.h>
#include <stdio.h>

#define MAX_N_THREADS (1024)

typedef void *(*ThreadFuncType)(void *);

ThreadFuncType thr_funcs[MAX_N_THREADS];
pthread_t children[MAX_N_THREADS];

void *worker_1(void *arg) {
  return NULL;
}

void *worker_2(void *arg) {
  return NULL;
}

int main(int argc, char *argv[]) {
  size_t n_threads = 0;
  thr_funcs[n_threads++] = worker_1;
  thr_funcs[n_threads++] = worker_2;

  for (size_t i = 0; i < n_threads; ++i) {
    pthread_t child;
    pthread_create(&child, NULL, thr_funcs[i], NULL);
    children[i] = child;
  }

  for (size_t i = 0; i < n_threads; ++i)
    pthread_join(children[i], NULL);

  return 0;
}
