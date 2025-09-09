#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include "sv.h"

typedef struct Hashmap {
    struct Hashmap *child[4];
    String_View key;
    String_View value;
} Hashmap;

// Funciones expuestas
uint64_t hash(String_View s);
int equals(String_View a, String_View b);
String_View *upsert(Hashmap **m, String_View key);

#endif // Hashmap_H
