#include <unistd.h>

int main() {
  write(1, "A\n", 2);
  write(1, "B\n", 2);
  while (1) { }
  return 0;
}
