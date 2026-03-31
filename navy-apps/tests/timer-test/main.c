#include <stdio.h>
#include <NDL.h>

int main() {
  NDL_Init(0);

  uint32_t last = NDL_GetTicks();
  uint32_t start = last;

  while (1) {
    uint32_t now = NDL_GetTicks();
    if (now - last >= 500) {
      printf("0.5s passed, ticks = %u ms\n", now - start);
      last += 500;
    }
  }

  return 0;
}
