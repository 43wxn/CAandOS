#include <stdio.h>
#include <assert.h>
#include <sys/types.h>

int main() {
  FILE *fp = fopen("/share/files/num", "r+");
  assert(fp);

  printf("sizeof(long)=%d sizeof(off_t)=%d\n",
         (int)sizeof(long), (int)sizeof(off_t));

  int r1 = fseek(fp, 0, SEEK_END);
  printf("fseek end ret = %d\n", r1);

  long t1 = ftell(fp);
  printf("ftell after SEEK_END = %ld\n", t1);

  int r2 = fseek(fp, 0, SEEK_CUR);
  printf("fseek cur ret = %d\n", r2);

  long t2 = ftell(fp);
  printf("ftell after SEEK_CUR = %ld\n", t2);

  assert(t1 == 5000);
  assert(t2 == 5000);

  fseek(fp, 500 * 5, SEEK_SET);
  int i, n;
  for (i = 500; i < 1000; i++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1);
  }

  fseek(fp, 0, SEEK_SET);
  for (i = 0; i < 500; i++) {
    fprintf(fp, "%4d\n", i + 1 + 1000);
  }

  for (i = 500; i < 1000; i++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1);
  }

  fseek(fp, 0, SEEK_SET);
  for (i = 0; i < 500; i++) {
    fscanf(fp, "%d", &n);
    assert(n == i + 1 + 1000);
  }

  fclose(fp);

  printf("PASS!!!\n");
  return 0;
}
