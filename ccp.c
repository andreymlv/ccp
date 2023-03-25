#include "client.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage() {
  printf("ccp -c <file> <ip> <port>\n");
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
    if (argc != 5) {
      usage();
      exit(EXIT_FAILURE);
    }
    init_client(&client, argv[2], argv[3], argv[4]);
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
