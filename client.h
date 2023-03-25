#ifndef CLIENT_H
#define CLIENT_H

typedef struct {
  char *file;
  int fd;
  char *ip;
  char *port;
} client;

void init_client(client *client, const char *file, const char *ip,
                 const char *port);

void free_client(client *client);

void open_read_send_client(client *client);

#endif // !CLIENT_H
