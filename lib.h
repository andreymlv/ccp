#ifndef LIB_H
#define LIB_H

#include <sys/socket.h>

#ifndef BUF_SIZE
#define BUF_SIZE 4096
#endif

#define BACKLOG 10 // how many pending connections queue will hold

void sigchld_handler(int s);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

#endif // !LIB_H
