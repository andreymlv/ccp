#include "server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "lib.h"

#define clock_duration(start, stop)         \
  (((double)(stop.tv_sec - start.tv_sec)) + \
   ((double)(stop.tv_nsec - start.tv_nsec)) / 1000000000)

void init_server(server *server, const char *port) {
  server->port = calloc(strlen(port) + 1, sizeof(char));
  if (server->port == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  strcpy(server->port, port);
}

void free_server(server *server) { free(server->port); }

void recv_server(server *server) {
  int sockfd = -1, new_fd = -1;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // use my IP

  if ((rv = getaddrinfo(NULL, server->port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);  // all done with this structure

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = sigchld_handler;  // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  printf("server: waiting for connections...\n");

  while (1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) {    // this is the child process
      close(sockfd);  // child doesn't need the listener

      int ret;
      unsigned have;
      z_stream strm;
      unsigned char in[BUF_SIZE];
      unsigned char out[BUF_SIZE];

      /* allocate inflate state */
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;
      strm.opaque = Z_NULL;
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      ret = inflateInit(&strm);
      if (ret != Z_OK) {
        perror("inflateInit");
      }

      struct timespec start, stop;

      memset(&start, 0, sizeof(start));
      memset(&stop, 0, sizeof(stop));

      clock_gettime(CLOCK_REALTIME, &start);
      do {
        strm.avail_in = recv(new_fd, in, BUF_SIZE, 0);
        // printf("recv %d\n", strm.avail_in);
        if (strm.avail_in == 0 || strm.avail_in == -1) {
          perror("recv");
          break;
        }
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
          strm.avail_out = BUF_SIZE;
          strm.next_out = out;
          ret = inflate(&strm, Z_NO_FLUSH);
          assert(ret != Z_STREAM_ERROR); /* state not clobbered */
          switch (ret) {
            case Z_NEED_DICT:
              ret = Z_DATA_ERROR; /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
              (void)inflateEnd(&strm);
          }
          have = BUF_SIZE - strm.avail_out;
          // printf("%.*s", (int)have, out);
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
      } while (ret != Z_STREAM_END);

      (void)inflateEnd(&strm);

      clock_gettime(CLOCK_REALTIME, &stop);

      printf("read with %f\n", clock_duration(start, stop));

      close(new_fd);
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }
}
