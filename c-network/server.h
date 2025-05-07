#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

extern const int BACKLOG;
extern const char *HOST;
extern const char *PORT;

void *get_in_addr(struct sockaddr *sa);
int setup_server_socket();

#endif // SERVER_H
