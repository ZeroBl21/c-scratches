#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const int BACKLOG = 10;
const char *HOST = NULL;
const char *PORT = "3490";

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char **argv) {
  puts("Hello world");

  struct addrinfo hints, *serv_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo(HOST, PORT, &hints, &serv_info);
  if (err != 0) {
    fprintf(stderr, "ERROR: getaddrinfo error: %s", gai_strerror(err));
    return 1;
  }

  int sockfd;
  struct addrinfo *p;
  for (p = serv_info; p != NULL; p = p->ai_next) {
    sockfd = socket(serv_info->ai_family, serv_info->ai_socktype,
                    serv_info->ai_protocol);
    if (sockfd < 0) {
      perror("SERVER ERROR: socket fd");
      continue;
    }

    int yes = 1;
    err = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (err < 0) {
      perror("SERVER ERROR: setsockopt");
      return 1;
    }

    err = bind(sockfd, serv_info->ai_addr, serv_info->ai_addrlen);
    if (err < 0) {
      perror("SERVER ERROR: socket bind error");
      continue;
    }

    break;
  }

  freeaddrinfo(serv_info);

  if (p == NULL) {
    fprintf(stderr, "SERVER ERROR: failed to bind");
    return 1;
  }

  err = listen(sockfd, BACKLOG);
  if (err < 0) {
    perror("SERVER ERROR: socket listen");
    return 1;
  }

  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("SERVER ERROR: sigaction");
    return 1;
  }

  printf("SERVER INFO: waiting for connections...\n");

  for (;;) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    int client_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (client_fd < 0) {
      fprintf(stderr, "ERROR: socket accept error\n");
      return 1;
    }

    char client_info[INET6_ADDRSTRLEN];
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              client_info, sizeof client_info);
    printf("SERVER INFO: got connection from %s\n", client_info);

    // INFO: This is the child process
    if (!fork()) {
      close(sockfd);

      char *msg = "There is a message bro\n";
      int bytes_sent = send(client_fd, msg, strlen(msg), 0);
      if (bytes_sent < 0) {
        perror("SERVER ERROR: socket (send)ing message error");
        return 1;
      }
      close(client_fd);
      return 0;
    }

    close(client_fd);
  }

  return EXIT_SUCCESS;
}

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
