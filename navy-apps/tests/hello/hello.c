#include <unistd.h>
#include <stdio.h>

int main() {
  write(1, "HELLO_MARKER_20260401\n", 22);
  int i = 2;
  volatile int j = 0;
  while (1) {
    j ++;
    if (j == 10000) {
      printf("LOOP_MARKER_%d\n", i ++);
      j = 0;
    }
  }
  return 0;
}
