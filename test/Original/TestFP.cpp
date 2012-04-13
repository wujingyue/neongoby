#include <pthread.h>

#include <iostream>
#include <vector>

typedef void *(*ThreadFuncType)(void *);

std::vector<ThreadFuncType> thr_funcs;

void *worker_1(void *arg) {
  return NULL;
}

void *worker_2(void *arg) {
  return NULL;
}

int main(int argc, char *argv[]) {
  thr_funcs.push_back(worker_1);
  thr_funcs.push_back(worker_2);

  std::vector<pthread_t> children;
  for (size_t i = 0; i < thr_funcs.size(); ++i) {
    pthread_t child;
    pthread_create(&child, NULL, thr_funcs[i], NULL);
    children.push_back(child);
  }

  for (size_t i = 0; i < children.size(); ++i)
    pthread_join(children[i], NULL);

  return 0;
}
