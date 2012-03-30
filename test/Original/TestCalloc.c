#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
  int n_members, member_size;
  int *a;

  assert(argc > 2);
  n_members = atoi(argv[1]);
  member_size = atoi(argv[2]);

  a = (int *)calloc(n_members, member_size);
  free(a);
  
  return 0;
}
