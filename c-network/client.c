#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const char *PORT = "3490";
const int MAX_DATA_SIZE = 100;

int setup_client_socket(char *network, char *port);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char **argv) {
  puts("Hello Client");

  if (argc != 2) {
    fprintf(stderr, "usage client <hostname>\n");
    return 1;
  }

  int sockfd = setup_client_socket(argv[1], (char *)PORT);
  if (sockfd < 0) {
    return 1;
  }

  char buf[MAX_DATA_SIZE];
  int num_bytes = recv(sockfd, buf, MAX_DATA_SIZE - 1, 0);
  if (num_bytes < 0) {
    perror("CLIENT ERROR: socket recv");
  }
  buf[num_bytes] = '\0';

  printf("CLIENT INFO: received '%s'\n", buf);

  close(sockfd);

  return EXIT_SUCCESS;
}

int setup_client_socket(char *network, char *port) {
  struct addrinfo hints, *serv_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(network, PORT, &hints, &serv_info);
  if (err != 0) {
    fprintf(stderr, "ERROR: getaddrinfo error: %s", gai_strerror(err));
    return -1;
  }

  int sockfd;
  struct addrinfo *p;
  for (p = serv_info; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) {
      perror("CLIENT ERROR: socket fd");
      continue;
    }

    err = connect(sockfd, p->ai_addr, p->ai_addrlen);
    if (err < 0) {
      perror("CLIENT ERROR: socket connect error");
      close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(serv_info);

  if (p == NULL) {
    fprintf(stderr, "CLIENT ERROR: failed to connect");
    return -1;
  }

  char server_info[INET6_ADDRSTRLEN];
  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            server_info, sizeof(server_info));
  printf("CLIENT INFO: connecting to %s\n", server_info);

  return sockfd;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
