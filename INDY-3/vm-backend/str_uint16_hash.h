#pragma once

/*
Uhhh..
hashmap_init()
hashmap_free()

hashmap_insert_kvp(char *key, uint16_t value)
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define KEYLEN_MAX 50

typedef struct kvp{
    char key[KEYLEN_MAX];
    uint16_t value;
    bool is_empty;
} kvp;

typedef struct hashmap {
    size_t size;
    kvp* data;

    size_t total_items;
    float load_factor;
} hashmap;

#pragma region Creation and Destruction

hashmap* hashmap_init(size_t initial_size);
void hashmap_free(hashmap *map);

#pragma endregion

void hashmap_insert_kvp(hashmap *map, char *key, uint16_t value);

uint16_t hashmap_get_value(hashmap *map, char *key, int *out_index);

void print_hashmap(hashmap *map);
