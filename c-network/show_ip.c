#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char **argv) {
  puts("Hello world");

  int status;
  struct addrinfo hints, *res, *p;
  char ip_str[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "USAGE: showip <hostname>\n");
    return 1;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(argv[1], NULL, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "ERROR: getaddrinfo error: %s\n", gai_strerror(status));
    return 1;
  }

  printf("IP addressees for %s\n\n", argv[1]);

  for (p = res; p != NULL; p = p->ai_next) {
    void *addr;
    char *ip_ver;

    if (p->ai_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;

      addr = &ipv4->sin_addr;
      ip_ver = "IPv4";
    } else {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;

      addr = &ipv6->sin6_addr;
      ip_ver = "IPv6";
    }

    inet_ntop(p->ai_family, addr, ip_str, sizeof(ip_str));
    printf("  %s: %s\n", ip_ver, ip_str);
  }

  freeaddrinfo(res);

  return EXIT_SUCCESS;
}
