#ifndef SV_H
#define SV_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#define DECLTYPE_CAST(T) (decltype(T))
#else
#define DECLTYPE_CAST(T)
#endif // __cplusplus

#define DA_INIT_CAP 256

// ------------------ Dynamic Array Macros ------------------

#define da_reserve(da, expected_capacity)                                      \
  do {                                                                         \
    if ((expected_capacity) > (da)->capacity) {                                \
      if ((da)->capacity == 0) {                                               \
        (da)->capacity = DA_INIT_CAP;                                          \
      }                                                                        \
      while ((expected_capacity) > (da)->capacity) {                           \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      (da)->items = DECLTYPE_CAST((da)->items)                                 \
          realloc((da)->items, (da)->capacity * sizeof(*(da)->items));         \
      assert((da)->items != NULL && "Buy more RAM lol");                       \
    }                                                                          \
  } while (0)

#define da_append(da, item)                                                    \
  do {                                                                         \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

#define da_append_many(da, new_items, new_items_count)                         \
  do {                                                                         \
    da_reserve((da), (da)->count + (new_items_count));                         \
    memcpy((da)->items + (da)->count, (new_items),                             \
           (new_items_count) * sizeof(*(da)->items));                          \
    (da)->count += (new_items_count);                                          \
  } while (0)

// ------------------ String Builder & View Macros ------------------

typedef struct {
  char *items;
  size_t count;
  size_t capacity;
} String_Builder;

typedef struct {
  const char *data;
  size_t count;
} String_View;

#define sb_append_cstr(sb, cstr)                                               \
  do {                                                                         \
    const char *s = (cstr);                                                    \
    size_t n = strlen(s);                                                      \
    da_append_many(sb, s, n);                                                  \
  } while (0)

#define sb_append_null(sb) da_append_many(sb, "", 1)

// Free the memory allocated by a string builder
#define sb_free(sb) free((sb).items)

#define sb_to_sv(sb) sv_from_parts((sb).items, (sb).count)

#ifndef SV_Fmt
#define SV_Fmt "%.*s"
#endif // SV_Fmt
#ifndef SV_Arg
#define SV_Arg(sv) (int)(sv).count, (sv).data
#endif // SV_Arg

// ------------------ Function Declarations ------------------

#ifdef __cplusplus
extern "C" {
#endif

// ---------- String Builder ----------
int sb_appendf(String_Builder *sb, const char *fmt, ...);
void sb_path_clean(String_Builder *sb, String_View path);
void sb_path_clean_absolute(String_Builder *sb, String_View path);

// ---------- String View creation ----------
String_View sv_from_parts(const char *data, size_t count);
String_View sv_from_cstr(const char *cstr);

// ---------- String View manipulation ----------
String_View sv_chop_left(String_View *sv, size_t n);
String_View sv_chop_by_delim(String_View *sv, char delim);
String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
String_View sv_trim(String_View sv);
String_View sv_substr(const String_View *sv, size_t start, size_t len);

String_View sv_to_lower_sb(String_Builder *sb, String_View sv);
String_View sv_to_upper_sb(String_Builder *sb, String_View sv);

// ---------- String View comparison ----------
bool sv_eq(String_View a, String_View b);
bool sv_end_with(String_View sv, const char *cstr);
bool sv_starts_with(String_View sv, String_View expected_prefix);

// ---------- Numeric conversion ----------
bool sv_to_i64(String_View sv, int64_t *out);
bool sv_to_i32(String_View sv, int32_t *out);

// ---------- UTF-8 ----------
uint32_t utf8_decode(const char *s, size_t len, size_t *consumed);
size_t utf8_encode(uint32_t cp, char out[4]);
size_t utf8_len(const char *data, size_t nbytes);

#ifdef __cplusplus
}
#endif

#endif // SV_H
