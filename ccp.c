#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "server.h"

static void usage(void) {
  printf("ccp -c <file> <level 0..9> <ip> <port>\n");
  printf("or\n");
  printf("ccp -s <port>\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    usage();
    exit(EXIT_FAILURE);
  }
  server server;
  client client;
  if (strcmp(argv[1], "-c") == 0) {
    // client mode
    if (argc != 6) {
      usage();
      exit(EXIT_FAILURE);
    }
    char *endptr, *str;
    long level;

    str = argv[3];

    errno = 0; /* чтобы выявить ошибку после вызова */
    level = strtol(str, &endptr, 10);

    /* проверка возможных ошибок */
    if ((errno == ERANGE && (level == LONG_MAX || level == LONG_MIN)) ||
        (errno != 0 && level == 0)) {
      perror("strtol");
      exit(EXIT_FAILURE);
    }

    if (endptr == str) {
      fprintf(stderr, "Цифры отсутствуют\n");
      exit(EXIT_FAILURE);
    }
    client.level = level;
    init_client(&client, argv[2], argv[4], argv[5]);
    open_read_send_client(&client);
    free_client(&client);
  } else if (strcmp(argv[1], "-s") == 0) {
    // server mode
    if (argc != 3) {
      usage();
      exit(EXIT_FAILURE);
    }
    init_server(&server, argv[2]);
    printf("Starting server at port: %s...\n", server.port);
    recv_server(&server);
    free_server(&server);
  } else {
    usage();
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
