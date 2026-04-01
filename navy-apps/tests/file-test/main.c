#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
  FILE *fp = fopen("/share/files/num", "r+");
  assert(fp);

  int fd = fileno(fp);
  printf("fileno = %d\n", fd);

  int ret = fseek(fp, 500 * 5, SEEK_SET);
  printf("fseek ret = %d\n", ret);

  char buf[33];
  memset(buf, 0, sizeof(buf));
  size_t nr = fread(buf, 1, 32, fp);

  printf("after fread: nr=%d feof=%d ferror=%d errno=%d\n",
         (int)nr, feof(fp), ferror(fp), errno);

  printf("buf from fread: [");
  for (int i = 0; i < (int)nr; i++) {
    char c = buf[i];
    if (c == '\n') printf("\\n");
    else putchar(c);
  }
  printf("]\n");

  // 再直接走低级接口验证一次
  ret = lseek(fd, 500 * 5, SEEK_SET);
  printf("lseek(fd) ret = %d\n", ret);

  memset(buf, 0, sizeof(buf));
  int nread = read(fd, buf, 32);
  printf("after read: nread=%d errno=%d\n", nread, errno);

  printf("buf from read: [");
  for (int i = 0; i < nread; i++) {
    char c = buf[i];
    if (c == '\n') printf("\\n");
    else putchar(c);
  }
  printf("]\n");

  return 0;
}
