// Author: Jingyue

#include <pthread.h>

#include <cstdio>
using namespace std;

static const unsigned N_ITERS = 1000000;

volatile unsigned counter = 0;

void *task(void *arg) {
  for (unsigned i = 0; i < N_ITERS; ++i)
    ++counter;
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t child1, child2;
  pthread_create(&child1, NULL, task, NULL);
  pthread_create(&child2, NULL, task, NULL);
  pthread_join(child1, NULL);
  pthread_join(child2, NULL);

  printf("counter = %u\n", counter);

  return 0;
}
