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

const char *HOST = NULL;
const char *PORT = "4950";
const int MAX_BUF_LEN = 100;

int setup_udp_server_socket();
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char **argv) {
  puts("Hello world");

  int sockfd = setup_udp_server_socket();
  if (sockfd < 0) {
    return 1;
  }

  printf("SERVER INFO: waiting for packets...\n");

  char buf[MAX_BUF_LEN];
  struct sockaddr_storage their_addr;
  socklen_t addr_size = sizeof(their_addr);

  int num_bytes = recvfrom(sockfd, buf, MAX_BUF_LEN - 1, 0,
                           (struct sockaddr *)&their_addr, &addr_size);
  if (num_bytes < 0) {
    perror("SERVER ERROR: socket recvfrom");
    return 1;
  }

  char client_info[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
            client_info, sizeof client_info);

  printf("SERVER INFO: got packet from %s\n", client_info);
  printf("SERVER INFO: packet received '%d' bytes long \n", num_bytes);
  buf[num_bytes] = '\0';
  printf("SERVER INFO: packet contains \"%s\"\n", buf);

  close(sockfd);

  return EXIT_SUCCESS;
}

int setup_udp_server_socket() {
  struct addrinfo hints, *serv_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo(HOST, PORT, &hints, &serv_info);
  if (err != 0) {
    fprintf(stderr, "ERROR: getaddrinfo error: %s", gai_strerror(err));
    return -1;
  }

  int sockfd;
  struct addrinfo *p;
  for (p = serv_info; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) {
      perror("SERVER ERROR: socket fd");
      continue;
    }

    err = bind(sockfd, p->ai_addr, p->ai_addrlen);
    if (err < 0) {
      close(sockfd);
      perror("SERVER ERROR: socket bind error");
      continue;
    }

    break;
  }

  freeaddrinfo(serv_info);

  if (p == NULL) {
    fprintf(stderr, "SERVER ERROR: failed to bind socket\n");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
