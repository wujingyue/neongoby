#include <pthread.h>
#include <unistd.h>

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
  int ret = fork();
  if (ret == 0) {
    // child
    printf("I'm child\n");
    task(NULL);
  } else {
    printf("I'm parent\n");
    task(NULL);
  }

  printf("counter = %u\n", counter);

  return 0;
}
