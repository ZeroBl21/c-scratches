#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "sv.h"

int sb_appendf(String_Builder *sb, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  // NOTE: the new_capacity needs to be +1 because of the null terminator.
  // However, further below we increase sb->count by n, not n + 1.
  // This is because we don't want the sb to include the null terminator. The
  // user can always sb_append_null() if they want it
  da_reserve(sb, sb->count + n + 1);
  char *dest = sb->items + sb->count;
  va_start(args, fmt);
  vsnprintf(dest, n + 1, fmt, args);
  va_end(args);

  sb->count += n;

  return n;
}

String_View sv_from_parts(const char *data, size_t count) {
  String_View sv;
  sv.data = data;
  sv.count = count;
  return sv;
}

String_View sv_from_cstr(const char *cstr) {
  return sv_from_parts(cstr, strlen(cstr));
}

String_View sv_chop_left(String_View *sv, size_t n) {
  if (n > sv->count) {
    n = sv->count;
  }

  String_View result = sv_from_parts(sv->data, n);

  sv->data += n;
  sv->count -= n;

  return result;
}

String_View sv_chop_by_delim(String_View *sv, char delim) {
  size_t i = 0;
  while (i < sv->count && sv->data[i] != delim) {
    i += 1;
  }

  String_View result = sv_from_parts(sv->data, i);

  if (i < sv->count) {
    sv->count -= i + 1;
    sv->data += i + 1;
  } else {
    sv->count -= i;
    sv->data += i;
  }

  return result;
}

bool sv_eq(String_View a, String_View b) {
    if (a.count != b.count) {
        return false;
    } else {
        return memcmp(a.data, b.data, a.count) == 0;
    }
}

String_View sv_trim_left(String_View sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[i])) {
    i += 1;
  }

  return sv_from_parts(sv.data + i, sv.count - i);
}

String_View sv_trim_right(String_View sv) {
  size_t i = 0;
  while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
    i += 1;
  }

  return sv_from_parts(sv.data, sv.count - i);
}

String_View sv_trim(String_View sv) {
  return sv_trim_right(sv_trim_left(sv));
}

String_View sv_substr(const String_View *sv, size_t start, size_t len) {
  if (start > sv->count)
    start = sv->count;
  if (start + len > sv->count)
    len = sv->count - start;

  String_View sub;
  sub.data = sv->data + start;
  sub.count = len;
  return sub;
}

uint32_t utf8_decode(const char *s, size_t len, size_t *consumed) {
  if (len == 0) {
    *consumed = 0;
    return 0;
  }

  unsigned char c = (unsigned char)s[0];
  if (c < 0x80) { // 1 byte ASCII
    *consumed = 1;
    return c;
  } else if ((c >> 5) == 0x6 && len >= 2) { // 2 bytes
    *consumed = 2;
    return ((c & 0x1F) << 6) | (s[1] & 0x3F);
  } else if ((c >> 4) == 0xE && len >= 3) { // 3 bytes
    *consumed = 3;
    return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
  } else if ((c >> 3) == 0x1E && len >= 4) { // 4 bytes
    *consumed = 4;
    return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) |
           (s[3] & 0x3F);
  } else {
    *consumed = 1;
    return 0xFFFD; // replacement character
  }
}

size_t utf8_encode(uint32_t cp, char out[4]) {
  if (cp <= 0x7F) {
    out[0] = cp;
    return 1;
  } else if (cp <= 0x7FF) {
    out[0] = 0xC0 | ((cp >> 6) & 0x1F);
    out[1] = 0x80 | (cp & 0x3F);
    return 2;
  } else if (cp <= 0xFFFF) {
    out[0] = 0xE0 | ((cp >> 12) & 0x0F);
    out[1] = 0x80 | ((cp >> 6) & 0x3F);
    out[2] = 0x80 | (cp & 0x3F);
    return 3;
  } else if (cp <= 0x10FFFF) {
    out[0] = 0xF0 | ((cp >> 18) & 0x07);
    out[1] = 0x80 | ((cp >> 12) & 0x3F);
    out[2] = 0x80 | ((cp >> 6) & 0x3F);
    out[3] = 0x80 | (cp & 0x3F);
    return 4;
  }
  return 0;
}

size_t utf8_len(const char *data, size_t nbytes) {
  size_t i = 0;
  size_t count = 0;
  while (i < nbytes) {
    size_t consumed;
    uint32_t cp = utf8_decode(data + i, nbytes - i, &consumed);
    (void)cp;
    if (consumed == 0)
      break; // entrada inválida
    count++;
    i += consumed;
  }
  return count;
}
