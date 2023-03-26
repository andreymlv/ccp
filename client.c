#include "client.h"
#include "lib.h"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

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
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

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

  int ret, flush;
  unsigned have;
  z_stream strm;
  unsigned char in[BUF_SIZE];
  unsigned char out[BUF_SIZE];

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    perror("deflateInit");
  }

  /* compress until end of file */
  do {
    strm.avail_in = read(client->fd, in, BUF_SIZE);
    if (strm.avail_in == -1) {
      perror("read");
      goto exit;
    }
    flush = strm.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;

    /* run deflate() on input until output buffer not full, finish
       compression if all of source has been read in */
    do {
      strm.avail_out = BUF_SIZE;
      strm.next_out = out;
      ret = deflate(&strm, flush);   /* no bad return value */
      assert(ret != Z_STREAM_ERROR); /* state not clobbered */
      have = BUF_SIZE - strm.avail_out;
      ssize_t sended;
      while ((sended = send(sockfd, out, have, 0)) < have) {
        have -= sended;
      }
      printf("sended %ld bytes.\n", sended);
    } while (strm.avail_out == 0);
    assert(strm.avail_in == 0); /* all input will be used */

    /* done when last data in file processed */
  } while (flush != Z_FINISH);
  assert(ret == Z_STREAM_END); /* stream will be complete */

  /* clean up and return */
  (void)deflateEnd(&strm);

exit:
  close(sockfd);
  if (close(client->fd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }
}
