#include <unistd.h>
#include <fcntl.h>

static void putnum(int x) {
  char buf[32];
  int n = 0;
  if (x == 0) { write(1, "0\n", 2); return; }
  if (x < 0) { write(1, "-", 1); x = -x; }
  while (x > 0) { buf[n++] = '0' + (x % 10); x /= 10; }
  for (int i = n - 1; i >= 0; i--) write(1, &buf[i], 1);
  write(1, "\n", 1);
}

int main() {
  int fd = open("/share/files/num", O_RDWR, 0666);
  putnum(fd);        // 先立刻打印
  write(1, "O2\n", 3);
  putnum(fd);        // 再打印一次
  while (1) { }
}
