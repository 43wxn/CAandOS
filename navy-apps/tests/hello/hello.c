#include <unistd.h>
#include <stdio.h>
int main() {
  write(1, "HELLO_MARKER_20260402\n", 22);
  printf("ceshi\n");
  write(1, "HELLO_MARKER_20260401\n", 22);
  int i = 2;
  write(1, "HELLO_MARKER_20260403\n", 22);
  volatile int j = 0;
  write(1, "HELLO_MARKER_20260404\n", 22);
  while (1) {
    j ++;
    if (j == 10000) {
      write(1, "LOOP\n", 5);
      j = 0;
    }
  }
  return 0;
}
