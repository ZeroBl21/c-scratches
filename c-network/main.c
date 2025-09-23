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

int main(int argc, char **argv) {
  puts("Working...");

  struct timeval tv;
  fd_set read_fds;

  tv.tv_sec = 2;
  tv.tv_usec = 500000;

  FD_ZERO(&read_fds);
  FD_SET(STDIN_FILENO, &read_fds);

  select(STDIN_FILENO +1, &read_fds, NULL, NULL, &tv);

  if (FD_ISSET(STDIN_FILENO, &read_fds))
    printf("A key was pressed!\n");
  else
    printf("Timed out\n");

  return EXIT_SUCCESS;
}
