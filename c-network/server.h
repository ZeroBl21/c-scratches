#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

void *get_in_addr(struct sockaddr *sa);
int setup_server_socket(char *host, char *port, int backlog);

#endif // SERVER_H
