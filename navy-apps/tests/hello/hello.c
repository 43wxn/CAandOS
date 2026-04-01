#include <unistd.h>
#include <stdio.h>
int main() {
  printf("ceshi\n");
  write(1, "HELLO_MARKER_20260401\n", 22);
  int i = 2;
  volatile int j = 0;
  write(1, "LOOP\n", 5);
  while (1) {
    j ++;
    if (j == 10000) {
      write(1, "LOOP\n", 5);
      j = 0;
    }
  }
  return 0;
}
