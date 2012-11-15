#include <iostream>
using namespace std;

struct A {
  A(): data(1) {}
  void print() {
    cout << data << "\n";
  }
 private:
  int data;
};

A ins;

int main() {
  ins.print();
  return 0;
}
