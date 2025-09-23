#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "sv.h"

// ---------- String Builder ----------

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

// ---------- String View creation ----------

String_View sv_from_parts(const char *data, size_t count) {
  String_View sv;
  sv.data = data;
  sv.count = count;
  return sv;
}

String_View sv_from_cstr(const char *cstr) {
  return sv_from_parts(cstr, strlen(cstr));
}

// ---------- String View manipulation ----------

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

String_View sv_trim(String_View sv) { return sv_trim_right(sv_trim_left(sv)); }

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

// ---------- String View comparison ----------

bool sv_eq(String_View a, String_View b) {
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
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

// ---------- Numeric conversion ----------

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

// ---------- UTF-8 ----------

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
