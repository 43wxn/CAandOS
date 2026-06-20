/*
 * touch-test — minimal file creation test
 * Usage: run touch-test
 * Creates /hello.txt with known content, then exits.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
  printf("touch-test: opening /hello.txt with O_CREAT...\n");
  int fd = open("/hello.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
  printf("touch-test: fd = %d\n", fd);
  if (fd < 0) {
    printf("touch-test: FAILED to create file\n");
    return 1;
  }

  const char *msg = "Hello from touch-test!\nLine two.\n";
  ssize_t n = write(fd, msg, strlen(msg));
  printf("touch-test: wrote %d bytes\n", (int)n);

  int ret = close(fd);
  printf("touch-test: close returned %d\n", ret);

  printf("touch-test: SUCCESS — check 'ls /' for hello.txt\n");
  return 0;
}
