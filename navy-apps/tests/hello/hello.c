#include <unistd.h>
#include <stdio.h>

int main() {
  printf("ceshi\n");
  write(1, "HELLO\n", 6);
  while (1) { }
  return 0;
}
