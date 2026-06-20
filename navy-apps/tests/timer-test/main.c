#include <stdio.h>
#include <NDL.h>
#include <stdlib.h>

int main() {
  NDL_Init(0);

  uint32_t start = NDL_GetTicks();
  uint32_t last = start;

  for (int tick = 0; tick < 5; ) {
    uint32_t now = NDL_GetTicks();
    if (now - last >= 500) {
      printf("timer-test: 0.5s passed, ticks = %u ms\n", now - start);
      last += 500;
      tick++;
    }
  }

  printf("timer-test: done (5 ticks)\n");
  return 0;
}
