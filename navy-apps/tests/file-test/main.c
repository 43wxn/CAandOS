#include <stdio.h>
#include <assert.h>
#include <errno.h>

int main() {
  FILE *fp = fopen("/share/files/num", "r+");
  assert(fp);

  int r = fseek(fp, 0, SEEK_END);
  printf("fseek ret = %d\n", r);

  long size = ftell(fp);
  printf("ftell size = %ld\n", size);

  assert(size == 5000);
  return 0;
}
