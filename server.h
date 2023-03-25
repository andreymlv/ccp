#ifndef SERVER_H
#define SERVER_H

typedef struct {
  char *port;
} server;

void init_server(server *server, const char *port);

void free_server(server *server);

void recv_server(server *server);

#endif // !SERVER_H
