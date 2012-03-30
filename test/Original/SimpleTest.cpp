#include <iostream>
#include <cstdlib>
#include <cassert>
using namespace std;

int main(int argc, char *argv[]) {
  assert(argc > 1);
  int sz = atoi(argv[1]);

  int **a = new int*[sz];
  for (int i = 0; i < sz; ++i) {
    a[i] = new int;
    *a[i] = i;
  }
  for (int i = 0; i < sz; ++i)
    delete a[i];
  delete a;

  return 0;
}
