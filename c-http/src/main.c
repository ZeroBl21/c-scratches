#define _POSIX_C_SOURCE 200112L // importante: antes de los includes

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hashmap.h"
#include "sv.h"

int setup_server_socket(const char *host, const char *port, int backlog) {
  struct addrinfo hints;
  struct addrinfo *serv_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo(host, port, &hints, &serv_info);
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

    int yes = 1;
    err = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (err < 0) {
      perror("SERVER ERROR: setsockopt");
      close(sockfd);
      return -1;
    }

    err = bind(sockfd, p->ai_addr, p->ai_addrlen);
    if (err < 0) {
      perror("SERVER ERROR: socket bind error");
      close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(serv_info);

  if (p == NULL) {
    fprintf(stderr, "SERVER ERROR: failed to bind");
    close(sockfd);
    return -1;
  }

  if (listen(sockfd, backlog) < 0) {
    perror("SERVER ERROR: socket listen");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

const char *PORT = "3490";
const int BACKLOG = 10;

#define KB(n) (((uint64_t)(n)) << 10)
#define MB(n) (((uint64_t)(n)) << 20)
#define GB(n) (((uint64_t)(n)) << 30)
#define TB(n) (((uint64_t)(n)) << 40)

int find_double_crlf(const String_View *sv) {
  for (size_t i = 0; i + 3 < sv->count; i++) {
    if (sv->data[i] == '\r' && sv->data[i + 1] == '\n' &&
        sv->data[i + 2] == '\r' && sv->data[i + 3] == '\n') {
      return (int)i + 4; // posición justo después del delimitador
    }
  }
  return -1; // no encontrado
}

#define TODO(message)                                                          \
  do {                                                                         \
    fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message);         \
    abort();                                                                   \
  } while (0)

int log_error(const char *fmt, ...) {
  fprintf(stderr, "[ERROR]: ");

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fputc('\n', stderr);

  return -1; // código de error estándar
}

String_View sv_to_lower_sb(String_Builder *sb, String_View sv) {
  da_reserve(sb, sb->count + sv.count);
  char *dest = sb->items + sb->count;

  for (size_t i = 0; i < sv.count; i++) {
    dest[i] = (char)tolower((unsigned char)sv.data[i]);
  }

  sb->count += sv.count;

  return sv_from_parts(dest, sv.count);
}

String_View sv_to_upper_sb(String_View sv, String_Builder *sb) {
  da_reserve(sb, sb->count + sv.count);
  char *dest = sb->items + sb->count;

  for (size_t i = 0; i < sv.count; i++) {
    dest[i] = (char)toupper((unsigned char)sv.data[i]);
  }

  sb->count += sv.count;

  return sv_from_parts(dest, sv.count);
}

bool sv_end_with(String_View sv, const char *cstr) {
  size_t cstr_count = strlen(cstr);
  if (sv.count >= cstr_count) {
    size_t ending_start = sv.count - cstr_count;
    String_View sv_ending = sv_from_parts(sv.data + ending_start, cstr_count);
    return sv_eq(sv_ending, sv_from_cstr(cstr));
  }
  return false;
}

bool sv_starts_with(String_View sv, String_View expected_prefix) {
  if (expected_prefix.count <= sv.count) {
    String_View actual_prefix = sv_from_parts(sv.data, expected_prefix.count);
    return sv_eq(expected_prefix, actual_prefix);
  }

  return false;
}

void sb_path_clean(String_Builder *sb, String_View path) {
  sb->count = 0;

  if (path.count == 0) {
    sb_append_cstr(sb, ".");
    return;
  }

  bool rooted = (path.data[0] == '/');
  size_t read_idx = 0;

  if (rooted) {
    da_append(sb, '/');
    read_idx = 1;
  }

  while (read_idx < path.count) {
    if (path.data[read_idx] == '/') {
      read_idx++; // ignore repeated slashes
    } else if (path.data[read_idx] == '.' &&
               (read_idx + 1 == path.count || path.data[read_idx + 1] == '/')) {
      read_idx++; // ignore "."
    } else if (path.data[read_idx] == '.' && read_idx + 1 < path.count &&
               path.data[read_idx + 1] == '.' &&
               (read_idx + 2 == path.count || path.data[read_idx + 2] == '/')) {
      read_idx += 2;
      if (sb->count > 1) {
        // backtrack to the previous '/'
        sb->count--;
        while (sb->count > 0 && sb->items[sb->count - 1] != '/') {
          sb->count--;
        }
      } else if (!rooted) {
        // cannot backtrack, append ".."
        if (sb->count > 0 && sb->items[sb->count - 1] != '/') {
          da_append(sb, '/');
        }
        sb_append_cstr(sb, "..");
      }
    } else {
      // normal path segment
      if (sb->count > 0 && sb->items[sb->count - 1] != '/') {
        da_append(sb, '/');
      }
      while (read_idx < path.count && path.data[read_idx] != '/') {
        da_append(sb, path.data[read_idx]);
        read_idx++;
      }
    }
  }

  if (sb->count == 0) {
    da_append_many(sb, ".", 1);
  }
}

void sb_path_clean_absolute(String_Builder *sb, String_View path) {
  if (path.count == 0) {
    sb->count = 0;
    sb_append_cstr(sb, "/");
    return;
  }

  String_Builder tmp = {0};
  if (path.data[0] != '/') {
    sb_append_cstr(sb, "/");
  }
  da_append_many(&tmp, path.data, path.count);

  sb_path_clean(sb, sb_to_sv(tmp));

  // Restore '/' at the end
  if (path.data[path.count - 1] == '/' &&
      !(sb->count == 1 && sb->items[0] == '/')) {
    if (!(path.count == sb->count + 1 && sv_eq(path, sb_to_sv(*sb)))) {
      da_append(sb, '/');
    }
  }

  sb_free(tmp);
}

// Converts a String_View to int64_t (base 10).
// Returns true if valid, false on error (invalid character, overflow, or
// underflow).
bool sv_to_i64(String_View sv, int64_t *out) {
  if (sv.count == 0)
    return false;

  size_t i = 0;
  bool negative = false;

  if (sv.data[0] == '-') {
    negative = true;
    i++;
  } else if (sv.data[0] == '+') {
    i++;
  }

  if (i == sv.count)
    return false; // only a sign, no digits

  uint64_t result = 0;
  for (; i < sv.count; i++) {
    char c = sv.data[i];
    if (c < '0' || c > '9') {
      return false; // not a decimal digit
    }
    uint64_t digit = (uint64_t)(c - '0');

    // Overflow check: result * 10 + digit > UINT64_MAX
    if (result > (UINT64_MAX - digit) / 10) {
      errno = ERANGE;
      return false;
    }
    result = result * 10 + digit;
  }

  if (negative) {
    if (result > (uint64_t)INT64_MAX + 1)
      return false;
    *out = -(int64_t)result;
  } else {
    if (result > INT64_MAX)
      return false;
    *out = (int64_t)result;
  }

  return true;
}

// Helper for int32_t
bool sv_to_i32(String_View sv, int32_t *out) {
  int64_t tmp;
  if (!sv_to_i64(sv, &tmp))
    return false;
  if (tmp < INT32_MIN || tmp > INT32_MAX)
    return false;
  *out = (int32_t)tmp;
  return true;
}

void send_response(int client_fd, String_View version, int status,
                   const char *reason, const char *content_type,
                   String_View body, bool shouldClose) {
  String_Builder sb = {0};
  da_reserve(&sb, KB(4));

  // date RFC 1123 format
  char date[128];
  time_t now = time(NULL);
  struct tm *gmt = gmtime(&now);
  strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmt);

  // Status line
  sb_appendf(&sb, SV_Fmt " %d %s\r\n", SV_Arg(version), status, reason);

  // Required Headers
  sb_appendf(&sb, "Date: %s\r\n", date);
  sb_appendf(&sb, "Server: Z_CServer/0.1\r\n");
  sb_appendf(&sb, "Content-Length: %zu\r\n", body.count);
  sb_appendf(&sb, "Content-Type: %s\r\n",
             content_type ? content_type : "text/plain");

  // TODO: Check when close the Connection
  sb_appendf(&sb, "Connection: %s\r\n", shouldClose ? "close" : "keep-alive");

  // headers end
  sb_appendf(&sb, "\r\n");

  // send headers
  send(client_fd, sb.items, sb.count, 0);

  // Body
  if (body.data && body.count > 0) {
    send(client_fd, body.data, body.count, 0);
  }

  sb_free(sb);
}

void respond_201(int client_fd, String_View version, String_View body,
                 bool shouldClose) {
  // If body is NULL, we can send an empty response
  send_response(client_fd, version, 201, "Created", "text/plain", body,
                shouldClose);
}

void respond_400(int client_fd, String_View version) {
  send_response(client_fd, version, 400, "Bad Request", "text/plain",
                sv_from_cstr("400 Bad Request"), true);
}

void respond_404(int client_fd, String_View version) {
  send_response(client_fd, version, 404, "Not Found", "text/plain",
                sv_from_cstr("404 Not Found"), true);
}

void respond_500(int client_fd, String_View version) {
  send_response(client_fd, version, 500, "Internal Server Error", "text/plain",
                sv_from_cstr("500 Internal Server Error"), true);
}

#define return_defer(value)                                                    \
  do {                                                                         \
    result = (value);                                                          \
    goto defer;                                                                \
  } while (0)

bool read_entire_file(const char *path, String_Builder *sb) {
  bool result = true;

  FILE *f = fopen(path, "rb");
  size_t new_count = 0;
  long long m = 0;
  if (f == NULL)
    return_defer(false);
  if (fseek(f, 0, SEEK_END) < 0)
    return_defer(false);
#ifndef _WIN32
  m = ftell(f);
#else
  m = _ftelli64(f);
#endif
  if (m < 0)
    return_defer(false);
  if (fseek(f, 0, SEEK_SET) < 0)
    return_defer(false);

  new_count = sb->count + m;
  if (new_count > sb->capacity) {
    sb->items = DECLTYPE_CAST(sb->items) realloc(sb->items, new_count);
    assert(sb->items != NULL && "Buy more RAM lool!!");
    sb->capacity = new_count;
  }

  fread(sb->items + sb->count, m, 1, f);
  if (ferror(f)) {
    return_defer(false);
  }
  sb->count = new_count;

defer:
  if (!result)
    log_error("Could not read file %s: %s", path, strerror(errno));
  if (f)
    fclose(f);
  return result;
}

typedef struct {
  String_View key;
  String_View value;
} HTTP_Header;

typedef struct {
  HTTP_Header *items;
  size_t count;
  size_t capacity;
} HTTP_Headers;

typedef struct {
  String_View method;
  String_View request_uri;
  String_View version;

  int64_t content_len;
  String_View host;

  HTTP_Headers headers;
  Hashmap *headers_map;

  String_Builder body;
} HTTP_Request;

#define MAX_HEADER_SIZE (KB(8))    // each header
#define MAX_HEADERS_TOTAL (KB(32)) // total

#define MAX_CONTENT_LEN (MB(10)) // total

bool http_parse_request_line(HTTP_Request *request, String_View request_line) {
  request->method = sv_chop_by_delim(&request_line, ' ');
  request->request_uri = sv_chop_by_delim(&request_line, ' ');
  request->version = sv_chop_by_delim(&request_line, '\n');
  if (request->method.count == 0 || request->request_uri.count == 0 ||
      request->version.count == 0) {
    return false;
  }
  if (request->version.data[request->version.count - 1] == '\r') {
    request->version.count--;
  }

  return true;
}

bool http_parse_headers(HTTP_Request *request, String_Builder *buffer,
                        String_View header_lines) {
  size_t total_header_size = 0;
  while (header_lines.count > 0) {
    String_View line = sv_chop_by_delim(&header_lines, '\n');

    // End of headers
    if (line.count == 0 || (line.count == 1 && line.data[0] == '\r')) {
      return true;
    }
    if (line.data[line.count - 1] == '\r') {
      line.count--;
    }

    if (line.count > MAX_HEADER_SIZE) {
      log_error("Header line exceeds maximum size: %zu > %d", line.count,
                MAX_HEADER_SIZE);
      return false;
    }

    total_header_size += line.count;
    if (total_header_size > MAX_HEADERS_TOTAL) {
      log_error("Total headers exceed maximum allowed size: %zu > %d",
                total_header_size, MAX_HEADERS_TOTAL);
      return false;
    }

    String_View key = sv_chop_by_delim(&line, ':');
    key = sv_trim(key);
    line = sv_trim(line);
    if (key.count == 0 || line.count == 0) {
      log_error("invalid header line: missing key or value");
      return false;
    }

    HTTP_Header h = {.key = sv_to_lower_sb(buffer, key),
                     .value = sv_to_lower_sb(buffer, line)};
    da_append(&request->headers, h);
    *upsert(&request->headers_map, h.key) = h.value;
  }

  return true;
}

void hashmap_reset(Hashmap *m) {
  if (!m)
    return;

  m->key.count = 0;
  m->value.count = 0;

  for (int i = 0; i < 4; i++) {
    hashmap_reset(m->child[i]);
  }
}

int main(void) {
  int listener = setup_server_socket(NULL, PORT, 10);

  if (listener < 0) {
    log_error("error getting listening socket");
    return -1;
  }

  printf("Listening on port %s\n", PORT);

  for (;;) {
    // struct sockaddr_storage their_addr;
    // socklen_t addr_size = sizeof(their_addr);

    int client_fd = accept(listener, NULL, NULL);
    // accept(listener, (struct sockaddr *)&their_addr, &addr_size);
    if (client_fd < 0) {
      perror("SERVER ERROR: socket accept error");
      return 1;
    }

    String_Builder sb_recv = {0};
    da_reserve(&sb_recv, MB(4));
    HTTP_Headers headers = {0};
    String_Builder full_path = {0};
    String_Builder file = {0};
    for (bool shouldClose = false; !shouldClose;) {

      for (;;) {
        ssize_t n = recv(client_fd, sb_recv.items + sb_recv.count,
                         sb_recv.capacity - sb_recv.count, 0);
        if (n <= 0) {
          log_error("error or client close");
          shouldClose = true;
          goto defer;
        }
        sb_recv.count += (size_t)n;

        String_View sv_full = sb_to_sv(sb_recv);
        if (find_double_crlf(&sv_full) >= 0)
          break; // completed headers
      }

      printf("recv count %zu\nrecv capacity %zu\n", sb_recv.count,
             sb_recv.capacity);

      printf("--------------------------------------\n");
      printf("%.*s\n", (int)sb_recv.count, sb_recv.items);

      String_View sv = sb_to_sv(sb_recv);

      String_View request_line = sv_chop_by_delim(&sv, '\n');

      // TODO: Validate Method
      String_View method = sv_chop_by_delim(&request_line, ' ');
      if (method.count == 0) {
        log_error("missing method");
        respond_400(client_fd, sv_from_cstr("HTTP/1.0"));
        shouldClose = true;
        goto defer;
      }

      String_View path = sv_chop_by_delim(&request_line, ' ');
      if (path.count == 0) {
        log_error("missing path");
        respond_400(client_fd, sv_from_cstr("HTTP/1.0"));
        shouldClose = true;
        goto defer;
      }

      String_View version = sv_chop_by_delim(&request_line, '\n');
      if (version.count == 0) {
        log_error("missing HTTP version");
        respond_400(client_fd, sv_from_cstr("HTTP/1.0"));
        shouldClose = true;
        goto defer;
      }
      if (version.data[version.count - 1] == '\r') {
        version.count--;
      }

      Hashmap *headers_map = NULL;
      while (sv.count > 0) {
        String_View line = sv_chop_by_delim(&sv, '\n');
        if (line.count == 0 || (line.count == 1 && line.data[0] == '\r')) {
          break;
        }
        if (line.data[line.count - 1] == '\r') {
          line.count--;
        }

        String_View key = sv_chop_by_delim(&line, ':');
        key = sv_trim(key);
        line = sv_trim(line);
        if (key.count == 0 || line.count == 0) {
          printf("Key: " SV_Fmt " Value: " SV_Fmt "\n", SV_Arg(key),
                 SV_Arg(line));
          log_error("invalid header line: missing key or value");
          respond_400(client_fd, version);
          shouldClose = true;
          goto defer;
        }

        HTTP_Header h = {.key = sv_to_lower_sb(&sb_recv, key),
                         .value = sv_to_lower_sb(&sb_recv, line)};
        da_append(&headers, h);
        *upsert(&headers_map, h.key) = h.value;
      }

      printf("--------------------------------------\n");
      printf("Headers Count: %zu\n", headers.count);
      for (size_t i = 0; i < headers.count; i++) {
        printf("  " SV_Fmt ": " SV_Fmt "\n", SV_Arg(headers.items[i].key),
               SV_Arg(headers.items[i].value));
      }

      // TODO: HTTP 1.1 Check obligatory HOST header
      // Check Connection header and loop until close
      // This is to handle closing the connection
      // Golang for reference http/transfer.go:748:0

      // HTTP/1.0
      if (sv_eq(version, sv_from_cstr("HTTP/1.0"))) {
        shouldClose = true; // close by default
        String_View *connection =
            upsert(&headers_map, sv_from_cstr("connection"));
        if (connection && sv_eq(*connection, sv_from_cstr("keep-alive"))) {
          shouldClose = false;
        }
      }

      // // HTTP/1.1
      if (sv_eq(version, sv_from_cstr("HTTP/1.1"))) {
        // HTTP 1.1: Check obligatory HOST header
        String_View *host = upsert(&headers_map, sv_from_cstr("host"));
        if (!host || host->count == 0) {
          log_error("HTTP/1.1 request missing Host header");
          respond_400(client_fd, version);
          shouldClose = true;
          goto defer;
        }

        shouldClose = false; // persistent by default
        String_View *connection =
            upsert(&headers_map, sv_from_cstr("connection"));
        if (connection && sv_eq(*connection, sv_from_cstr("close"))) {
          shouldClose = true;
        }
      }

      // TODO: Maybe check the method and Transfer-Encoding: chunked
      // Check if Content-Length doesn't exceed the buffer
      // Content-Length Size discussion
      // https://stackoverflow.com/questions/2880722/can-http-post-be-limitless#55998160
      if (sv_eq(method, sv_from_cstr("POST"))) {
        // TODO: Move body read to a function
        // handle `Transfer-Encoding: chunked`
        String_View *cl_sv =
            upsert(&headers_map, sv_from_cstr("content-length"));
        // A valid Content-Length is required on all HTTP/1.0 POST requests.
        if (!cl_sv->data || sv.count == 0) {
          log_error("Missing Content-Length or Body");
          respond_400(client_fd, version);
          shouldClose = true;
          goto defer;
        }

        int64_t content_len;
        if (!sv_to_i64(*cl_sv, &content_len) ||
            content_len > (int64_t)MAX_CONTENT_LEN) {
          printf("Content-Length = %lu\n", content_len);
          log_error("Invalid number or too big");
          respond_400(client_fd, version); // invalid or too big
          shouldClose = true;
          goto defer;
        }

        printf("Content-Length = %lu\n", content_len);
        printf("sv count = %lu\n", sv.count);
        if (content_len != (int64_t)sv.count) {
          TODO("Handle missing bytes");
        }

        // If the media type remains unknown, the recipient should
        // treat it as type "application/octet-stream".
        printf("Body\n" SV_Fmt "\n", SV_Arg(sv));

        if (sv_eq(path, sv_from_cstr("/create"))) {
          // respond_201(client_fd, "HTTP/1.0", "Resource created
          // successfully");
          String_Builder response = {0};
          sb_appendf(&response, SV_Fmt, SV_Arg(sv));
          sb_append_null(&response);

          respond_201(client_fd, version, sb_to_sv(response), shouldClose);

          goto defer;
        }

        respond_404(client_fd, version);
        goto defer;
      }

      if (sv_eq(method, sv_from_cstr("GET"))) {
        if (sv_eq(path, sv_from_cstr("/"))) {
          send_response(client_fd, version, 200, "OK", "text/plain",
                        sv_from_cstr("Hello, world! From Home\n"), shouldClose);
          goto defer;
        }

        sb_appendf(&full_path, "./public" SV_Fmt, SV_Arg(path));
        sb_path_clean(&full_path, sb_to_sv(full_path));
        sb_append_null(&full_path);

        if (!read_entire_file(full_path.items, &file)) {
          respond_404(client_fd, version);
          goto defer;
        }

        // TODO: Make a extensions table
        const char *filetype = "text/plain";
        if (sv_end_with(path, ".html")) {
          filetype = "text/html";
        }
        if (sv_end_with(path, ".png")) {
          filetype = "image/png";
        }

        send_response(client_fd, version, 200, "OK", filetype, sb_to_sv(file),
                      shouldClose);

        goto defer;
      }

      // TODO: Check Golang as a reference API
      send_response(client_fd, version, 200, "OK", "text/plain",
                    sv_from_cstr("Hello, world!"), shouldClose);

    defer:
      sb_recv.count = 0;
      headers.count = 0;
      full_path.count = 0;
      file.count = 0;

      if (shouldClose) {
        break;
      }
    }

    printf("Closing Connection\n");

    sb_free(sb_recv);
    sb_free(headers);
    sb_free(full_path);
    sb_free(file);
    close(client_fd);
  }

  return 0;
}
