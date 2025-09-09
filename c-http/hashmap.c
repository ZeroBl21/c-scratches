#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

uint64_t hash(String_View s) {
  uint64_t h = 0x100;
  for (size_t i = 0; i < s.count; i++) {
    h ^= s.data[i];
    h *= 1111111111111111111ull;
  }
  return h;
}

// TODO: Maybe import sv.h
int equals(String_View a, String_View b) {
  return a.count == b.count && !memcmp(a.data, b.data, a.count);
}

String_View *upsert(Hashmap **m, String_View key) {
  for (uint64_t h = hash(key); *m; h <<= 2) {
    if (equals(key, (*m)->key)) {
      return &(*m)->value; // found
    }
    m = &(*m)->child[h >> 62];
  }
  // TODO: optional arena
  *m = calloc(1, sizeof(Hashmap)); // reserve
  (*m)->key = key;
  return &(*m)->value; // empty value
}
