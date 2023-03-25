#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef BUF_SIZE
#define BUF_SIZE 4096
#endif

#define BACKLOG 10 // how many pending connections queue will hold

typedef struct {
  char *file;
  int fd;
  char *ip;
  char *port;
} client_args;

typedef struct {
  char *port;
} server_args;

static void sigchld_handler(int s) {
  (void)s; // quiet unused variable warning

  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

static void init_client(client_args *client, const char *file, const char *ip,
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

static void free_client(client_args *client) {
  free(client->file);
  free(client->ip);
  free(client->port);
}

static void init_server(server_args *server, const char *port) {
  server->port = calloc(strlen(port) + 1, sizeof(char));
  if (server->port == NULL) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  strcpy(server->port, port);
}

static void free_server(server_args *server) { free(server->port); }

static long str_to_long(const char *str) {
  char *endptr;
  long number = strtol(str, &endptr, 0);
  if (errno != 0) {
    perror("strtol");
    exit(EXIT_FAILURE);
  }
  if (endptr == str) {
    fprintf(stderr, "No digits were found\n");
    exit(EXIT_FAILURE);
  }
  return number;
}

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

  server_args server;
  client_args client;

  if (strcmp(argv[1], "-c") == 0) {
    // client mode
    if (argc != 5) {
      usage();
      exit(EXIT_FAILURE);
    }
    init_client(&client, argv[2], argv[3], argv[4]);
    printf("Connect to %s:%s\n", client.ip, client.port);

    int sockfd, numbytes;
    char buf[BUF_SIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(client.ip, client.port, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
          -1) {
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
      return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
              sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    char buffer[BUF_SIZE];
    ssize_t readed;
    client.fd = open(client.file, O_RDONLY);
    if (client.fd == -1) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    while ((readed = read(client.fd, buffer, BUF_SIZE)) > 0) {
      ssize_t sended;
      // TODO: is there bug?
      while ((sended = send(sockfd, buffer, readed, 0)) < readed) {
        readed -= sended;
      }
      if (sended == -1) {
        perror("send");
      }

      // printf("\n\n\n=====\n\nsended %ld and readed %ld\n\n\n=====\n\n",
      // sended,
      //        readed);

      printf("%.*s", (int)readed, buffer);
    }
    if (readed == -1) {
      perror("read");
      goto exit;
    }
    printf("Sending %s\n", client.file);

    printf("client: received '%s'\n", buf);

    close(sockfd);

    free_client(&client);
  } else if (strcmp(argv[1], "-s") == 0) {
    // server mode
    if (argc != 3) {
      usage();
      exit(EXIT_FAILURE);
    }
    init_server(&server, argv[2]);
    printf("Starting server at port: %s...\n", server.port);

    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, server.port, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
          -1) {
        perror("server: socket");
        continue;
      }

      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
          -1) {
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

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
      fprintf(stderr, "server: failed to bind\n");
      exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) == -1) {
      perror("listen");
      exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
      perror("sigaction");
      exit(EXIT_FAILURE);
    }

    printf("server: waiting for connections...\n");

    while (1) { // main accept() loop
      sin_size = sizeof their_addr;
      new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
      if (new_fd == -1) {
        perror("accept");
        continue;
      }

      inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
      printf("server: got connection from %s\n", s);

      if (!fork()) {   // this is the child process
        close(sockfd); // child doesn't need the listener
        char buf[BUF_SIZE];
        ssize_t numbytes;
        while ((numbytes = recv(new_fd, buf, BUF_SIZE, 0)) > 0) {
          printf("%.*s", (int)numbytes, buf);
        }
        if (numbytes == -1) {
          perror("recv");
        }
        close(new_fd);
        exit(0);
      }
      close(new_fd); // parent doesn't need this
    }

    free_server(&server);
  } else {
    usage();
    exit(EXIT_FAILURE);
  }

exit:
  if (close(client.fd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
