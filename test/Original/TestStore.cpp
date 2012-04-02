// Author: Jingyue

#include <iostream>
using namespace std;

static const int MAX_N = 1024;

int *arr[MAX_N];
long num[MAX_N];

int main() {
  int target = 0;
  for (int i = 0; i < MAX_N; ++i) {
    arr[i] = &target;
    num[i] = i;
  }
  return 0;
}
