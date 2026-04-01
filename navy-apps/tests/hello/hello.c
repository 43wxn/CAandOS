#include <unistd.h>

int main() {
  write(1, "HELLO_MARKER_20260401\n", 22);
  int i = 2;
  write(1, "HELLO_MARKER_20260402\n", 22);
  volatile int j = 0;
  while (1) {
    j ++;
    if (j == 10000) {
      write(1, "LOOP\n", 5);
      j = 0;
    }
  }
  return 0;
}
