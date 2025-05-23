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

#define da_reserve(da, expected_capacity)                                      \
  do {                                                                         \
    if ((expected_capacity) > (da)->capacity) {                                \
      if ((da)->capacity == 0) {                                               \
        (da)->capacity = 256;                                                  \
      }                                                                        \
      while ((expected_capacity) > (da)->capacity) {                           \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      (da)->items =                                                            \
          realloc((da)->items, (da)->capacity * sizeof(*(da)->items));         \
    }                                                                          \
  } while (0)

#define da_append(da, item)                                                    \
  do {                                                                         \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

#define da_remove_unordered(da, i)                                             \
  do {                                                                         \
    size_t j = (i);                                                            \
    (da)->items[j] = (da)->items[--(da)->count];                               \
  } while (0)

typedef struct {
  struct pollfd *items;
  size_t count;
  size_t capacity;
} Clients;

static void handle_new_connection(int listener, Clients *clients);
static void handle_broadcast(int listener, Clients *clients, int sender_fd,
                             char *buf, int nbytes);

int main(int argc, char **argv) {
  puts("Starting chat...");

  int listener = setup_server_socket();
  if (listener == -1) {
    fprintf(stderr, "SERVER ERROR: error getting listening socket\n");
    return 1;
  }

  Clients clients = {0};
  da_append(&clients, ((struct pollfd){.fd = listener, .events = POLLIN}));

  for (;;) {
    int poll_count = poll(clients.items, clients.count, -1);
    if (poll_count == -1) {
      perror("SERVER ERROR: poll");
      return -1;
    }

    for (int i = 0; i < clients.count; i++) {
      if (!(clients.items[i].revents & (POLLIN | POLLHUP))) {
        continue;
      }

      if (clients.items[i].fd == listener) {
        handle_new_connection(listener, &clients);
        continue;
      }

      int sender_fd = clients.items[i].fd;
      char buf[256];
      int nbytes = recv(clients.items[i].fd, buf, sizeof(buf), 0);
      if (nbytes <= 0) {
        if (nbytes < 0) {
          perror("SERVER ERROR: error recv");
        } else {
          printf("SERVER INFO: socket %d hung up\n", sender_fd);
        }
        close(clients.items[i].fd);
        da_remove_unordered(&clients, i);
        i--;
        continue;
      }

      handle_broadcast(listener, &clients, sender_fd, buf, nbytes);
    }
  }

  return EXIT_SUCCESS;
}

static void handle_new_connection(int listener, Clients *clients) {
  struct sockaddr_storage remote_addr;
  socklen_t addr_len = sizeof remote_addr;

  int new_fd = accept(listener, (struct sockaddr *)&remote_addr, &addr_len);
  if (new_fd < 0) {
    perror("SERVER ERROR: error (accept)ing connection");
    return;
  }

  da_append(clients, ((struct pollfd){.fd = new_fd, POLLIN}));

  char remote_ip[INET6_ADDRSTRLEN];
  const char *client_info = inet_ntop(
      remote_addr.ss_family, get_in_addr((struct sockaddr *)&remote_addr),
      remote_ip, INET6_ADDRSTRLEN);
  printf("SERVER INFO: new connection from %s on socket %d\n", client_info,
         new_fd);

  return;
}

static void handle_broadcast(int listener, Clients *clients, int sender_fd,
                             char *buf, int nbytes) {
  char message[512];
  int msg_len =
      snprintf(message, sizeof message, "[%d]: %.*s", sender_fd, nbytes, buf);
  for (int j = 0; j < clients->count; j++) {
    int dest_fd = clients->items[j].fd;
    if (dest_fd != listener && dest_fd != sender_fd) {
      if (send(dest_fd, message, msg_len, 0) == -1) {
        perror("send");
      }
    }
  }
}
