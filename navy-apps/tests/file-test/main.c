#include <stdio.h>
#include <assert.h>
#include <unistd.h>

int main() {
  write(1, "M1\n", 3);

  FILE *fp = fopen("/share/files/num", "r+");

  write(1, "M2\n", 3);

  if (!fp) {
    write(1, "OPEN_NULL\n", 10);
    return 1;
  }

  write(1, "M3\n", 3);

  int r = fseek(fp, 0, SEEK_END);
  if (r != 0) {
    write(1, "FSEEK_FAIL\n", 11);
    return 2;
  }

  write(1, "M4\n", 3);

  long size = ftell(fp);
  if (size != 5000) {
    write(1, "FTELL_BAD\n", 10);
    return 3;
  }

  write(1, "M5\n", 3);

  fclose(fp);
  write(1, "PASS_STAGE1\n", 12);
  return 0;
}
