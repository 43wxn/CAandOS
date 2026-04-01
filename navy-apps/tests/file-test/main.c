#include <stdio.h>
#include <assert.h>
#include <string.h>

int main() {
  FILE *fp = fopen("/share/files/num", "r+");
  assert(fp);

  int ret = fseek(fp, 500 * 5, SEEK_SET);
  printf("fseek ret = %d\n", ret);

  char buf[33];
  memset(buf, 0, sizeof(buf));
  size_t nr = fread(buf, 1, 32, fp);

  printf("fread nr = %d\n", (int)nr);
  printf("raw bytes:\n");
  for (int i = 0; i < (int)nr; i++) {
    char c = buf[i];
    if (c == '\n') printf("\\n");
    else putchar(c);
  }
  putchar('\n');

  int n = -1;
  ret = sscanf(buf, "%d", &n);
  printf("sscanf ret = %d, n = %d\n", ret, n);

  return 0;
}
