#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "str_uint16_hash.h"
#include "leakcheck_util.h"

static void initialize_hashmap_data(hashmap *map){

    for(size_t i = 0; i < map->size; i++){
        strncpy(map->data[i].key, "", KEYLEN_MAX);
        map->data[i].value = 0;
        map->data[i].is_empty = true;
    }

}

static void print_kvp(kvp k){
    printf("%s : %d\n", k.key, k.value);
}

void print_hashmap(hashmap *map){
    printf("Total items: %d\nCapacity: %d\nPercent full: %.2f\n", (int)map->total_items, (int)map->size, (float)map->total_items / (float)map->size);
    for(size_t i = 0; i < map->size; i++){
        if(strcmp(map->data[i].key, "") != 0){
            printf("%d ", (int)i);
            print_kvp(map->data[i]);
        }
        else{
            printf("%d [...]\n", (int)i);
        }
    }
}

hashmap* hashmap_init(size_t initial_size){

    hashmap *map = malloc(sizeof(hashmap));
    //print_allocation_data();

    map->size = initial_size;
    map->data = malloc(sizeof(kvp) * initial_size);

    initialize_hashmap_data(map);

    map->total_items = 0;
    map->load_factor = 0.75f;
    return map;
}

void hashmap_free(hashmap *map){
    if(map){
        if(map->data) free(map->data);
        free(map);
    }
}

// Basic division method hash function.
// Adds up all of the characters in the string and then wraps them to within the hashtable size.
static size_t modulo_hash(hashmap *map, char *string){
    size_t result = 0;
    for(size_t i = 0; i < strlen(string); i++){
        result += string[i];
    }
    return result % map->size;
}

static void re_hash(hashmap *map){
    kvp *old_data = map->data; // Save the old data pointer for now. Will free it at the end of the function.

    size_t old_size = map->size;
    size_t new_size = old_size * 2;

    map->data = malloc(sizeof(kvp) * new_size);
    map->size = new_size;

    // zero out all of the new data
    initialize_hashmap_data(map);

    // Go through each item in old_size. If it's a non-empty kvp, hash it in the new map.
    for(size_t i = 0; i < old_size; i++){
        if(!old_data[i].is_empty){
            hashmap_insert_kvp(map, old_data[i].key, old_data[i].value);
        }
    }

    free(old_data);

    print_allocation_data();
}

void hashmap_insert_kvp(hashmap *map, char *key, uint16_t value){
    printf("Hashing key: %s value: %d.\n", key, value);
    size_t index = modulo_hash(map, key);

    int i = 0;
    while(!map->data[index].is_empty){ // Linear probing
        index = (index + 1) % map->size;
    }

    // Apply the key and value.
    strncpy(map->data[index].key, key, KEYLEN_MAX);
    map->data[index].value = value;
    map->data[index].is_empty = false;

    map->total_items++;

    if ((float)map->total_items / (float)map->size > map->load_factor){
        // double size and re-hash.
        re_hash(map);
    }
}

uint16_t hashmap_get_value(hashmap *map, char *key, int *out_index){
    size_t counted_items = 0;

    size_t index = modulo_hash(map, key);

    int i = 0;
    while(strncmp(key, map->data[index].key, KEYLEN_MAX) != 0){ // Linear probing
        index = (index + 1) % map->size;

        i++;

        if(i > map->size){
            printf("Item not found in hashmap.\n");
            return 0;
        }
    }

    *out_index = (int)index;
    return map->data[index].value;
}