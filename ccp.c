#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

static void usage() {
  printf("ccp <file>\n");
  // printf("ccp <file> <server-ip> <port>\n");
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    usage();
    exit(EXIT_FAILURE);
  }
  char buffer[BUF_SIZE];
  ssize_t readed;
  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("open");
    exit(EXIT_FAILURE);
  }
  while ((readed = read(fd, buffer, BUF_SIZE)) > 0) {
    printf("%.*s", (int)readed, buffer);
  }
  if (readed == -1) {
    perror("read");
    goto exit;
  }
exit:
  if (close(fd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
