#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#import "server.h"

void setup_sigchld_handler();
void sigchld_handler(int s);
void handle_client(int client_fd, struct sockaddr_storage their_addr);

int main(int argc, char **argv) {
  puts("Hello world");

  int sockfd = setup_server_socket();
  if (sockfd < 0) {
    return 1;
  }

  setup_sigchld_handler();

  printf("SERVER INFO: waiting for connections...\n");

  for (;;) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    int client_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (client_fd < 0) {
      perror("SERVER ERROR: socket accept error");
      return 1;
    }

    if (!fork()) {
      // INFO: This is the child process
      close(sockfd);
      handle_client(client_fd, their_addr);
      exit(0);
    }

    close(client_fd);
  }

  return EXIT_SUCCESS;
}

// INFO: This is the child process
void handle_client(int client_fd, struct sockaddr_storage their_addr) {
  char client_info[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
            client_info, sizeof client_info);

  printf("SERVER INFO: got connection from %s\n", client_info);

  char *msg = "There is a message bro";
  int bytes_sent = send(client_fd, msg, strlen(msg), 0);
  if (bytes_sent < 0) {
    perror("SERVER ERROR: socket (send)ing message error");
  }
  close(client_fd);
}

void setup_sigchld_handler() {
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("SERVER ERROR: sigaction");
    exit(1);
  }
}

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}
