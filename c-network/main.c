#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"

void handle_new_connection(int listener, struct pollfd **pfds, int *fd_count,
                           int *fd_size);
void append_to_pfds(struct pollfd *pfds[], int new_fd, int *fd_count,
                    int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

int main(int argc, char **argv) {
  puts("Starting chat...");

  int listener = setup_server_socket();
  if (listener == -1) {
    fprintf(stderr, "SERVER ERROR: error getting listening socket\n");
    return 1;
  }

  int fd_count = 0;
  int fd_size = 5;
  struct pollfd *pfds = malloc(sizeof *pfds * fd_size);
  pfds[0].fd = listener;
  pfds[0].events = POLLIN;
  fd_count = 1;

  char buf[256];
  for (;;) {
    int poll_count = poll(pfds, fd_count, -1);
    if (poll_count == -1) {
      perror("SERVER ERROR: poll");
      return -1;
    }

    for (int i = 0; i < fd_count; i++) {
      if (!(pfds[i].revents & (POLLIN | POLLHUP))) {
        continue;
      }

      if (pfds[i].fd == listener) {
        handle_new_connection(listener, &pfds, &fd_count, &fd_size);
        continue;
      }

      int sender_fd = pfds[i].fd;
      int nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
      if (nbytes <= 0) {
        if (nbytes < 0) {
          perror("SERVER ERROR: error recv");
        } else {
          printf("SERVER INFO: socket %d hung up\n", sender_fd);
        }
        close(pfds[i].fd);
        del_from_pfds(pfds, i, &fd_count);
        i--;
        continue;
      }

      char message[512];
      int msg_len = snprintf(message, sizeof message, "[%d]: %.*s", sender_fd,
                             nbytes, buf);
      for (int j = 0; j < fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
          if (send(dest_fd, message, msg_len, 0) == -1) {
            perror("send");
          }
        }
      }
    }
  }

  return EXIT_SUCCESS;
}

void handle_new_connection(int listener, struct pollfd **pfds, int *fd_count,
                           int *fd_size) {
  struct sockaddr_storage remote_addr;
  socklen_t addr_len = sizeof remote_addr;

  int new_fd = accept(listener, (struct sockaddr *)&remote_addr, &addr_len);
  if (new_fd < 0) {
    perror("SERVER ERROR: error (accept)ing connection");
    return;
  }

  append_to_pfds(pfds, new_fd, fd_count, fd_size);

  char remote_ip[INET6_ADDRSTRLEN];
  const char *client_info = inet_ntop(
      remote_addr.ss_family, get_in_addr((struct sockaddr *)&remote_addr),
      remote_ip, INET6_ADDRSTRLEN);
  printf("SERVER INFO: new connection from %s on socket %d\n", client_info,
         new_fd);

  return;
}

void append_to_pfds(struct pollfd *pfds[], int new_fd, int *fd_count,
                    int *fd_size) {
  if (*fd_count >= *fd_size) {
    *fd_size *= 2;

    pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
  }

  (*pfds)[*fd_count].fd = new_fd;
  (*pfds)[*fd_count].events = POLLIN;
  (*pfds)[*fd_count].revents = 0;

  (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
  pfds[i] = pfds[*fd_count - 1];

  (*fd_count)--;
}
