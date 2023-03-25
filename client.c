#include "client.h"
#include "lib.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_client(client *client, const char *file, const char *ip,
                 const char *port) {
  // TODO: write a function to minimize code duplication.
  client->file = calloc(strlen(file) + 1, sizeof(char));
  if (client->file == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  strcpy(client->file, file);

  client->ip = calloc(strlen(ip) + 1, sizeof(char));
  if (client->ip == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  strcpy(client->ip, ip);

  client->port = calloc(strlen(port) + 1, sizeof(char));
  if (client->port == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  strcpy(client->port, port);
}

void free_client(client *client) {
  free(client->file);
  free(client->ip);
  free(client->port);
}

void open_read_send_client(client *client) {
  int sockfd, numbytes;
  char buf[BUF_SIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  ssize_t readed;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(client->ip, client->port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      perror("client: connect");
      close(sockfd);
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    exit(EXIT_FAILURE);
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  if ((client->fd = open(client->file, O_RDONLY)) == -1) {
    perror("open");
    exit(EXIT_FAILURE);
  }

  printf("Sending %s\n", client->file);

  while ((readed = read(client->fd, buf, BUF_SIZE)) > 0) {
    ssize_t sended;
    // TODO: is there bug?
    while ((sended = send(sockfd, buf, readed, 0)) < readed) {
      readed -= sended;
    }
    if (sended == -1) {
      perror("send");
    }

    // printf("\n\n\n=====\n\nsended %ld and readed %ld\n\n\n=====\n\n",
    // sended,
    //        readed);

    printf("%.*s", (int)readed, buf);
  }
  if (readed == -1) {
    perror("read");
    goto exit;
  }

exit:
  close(sockfd);
  if (close(client->fd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }
}
