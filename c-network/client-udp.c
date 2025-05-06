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

const char *PORT = "4950";

int setup_udp_client_socket(char *network, char *port,
                            struct addrinfo **serv_addr);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char **argv) {
  puts("Hello Client");

  if (argc != 3) {
    fprintf(
        stderr,
        "Usage: talker <hostname> <message>\n"
        "  <hostname>  - Hostname or IP address of the receiver\n"
        "  <message>   - Message to send (use quotes if it contains spaces)\n");
    return 1;
  }

  struct addrinfo *serv_addr;
  int sockfd = setup_udp_client_socket(argv[1], (char *)PORT, &serv_addr);
  if (sockfd < 0) {
    return 1;
  }

  int num_bytes = sendto(sockfd, argv[2], strlen(argv[2]), 0,
                         serv_addr->ai_addr, serv_addr->ai_addrlen);
  if (num_bytes < 0) {
    perror("CLIENT ERROR: socket sendto");
    close(sockfd);
    return 1;
  }

  printf("CLIENT INFO: send %d bytes to %s\n", num_bytes, argv[1]);

  close(sockfd);

  return EXIT_SUCCESS;
}

int setup_udp_client_socket(char *network, char *port,
                            struct addrinfo **serv_addr) {
  struct addrinfo hints, *serv_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  int err = getaddrinfo(network, PORT, &hints, &serv_info);
  if (err != 0) {
    fprintf(stderr, "CLIENT ERROR: getaddrinfo error: %s", gai_strerror(err));
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

    break;
  }

  freeaddrinfo(serv_info);

  if (p == NULL) {
    fprintf(stderr, "CLIENT ERROR: could not create UDP socket\n");
    close(sockfd);
    return -1;
  }

  *serv_addr = p;

  return sockfd;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
