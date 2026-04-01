#include <unistd.h>
#include <stdio.h>
int main() {
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
