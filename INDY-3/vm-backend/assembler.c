/*	Implementation file for assembler.h
	Author: Matthew Watson
*/

#include <stdio.h>

#include "assembler.h"
#include "str_uint16_hash.h"
#include "linklist.h"

#include "leakcheck_util.h"

#pragma region Byte List
byte_list* byte_list_init(size_t init_size) {
	byte_list* b = malloc(sizeof(byte_list));
	b->size = init_size;
	b->list = malloc(init_size);
	b->index = 0;

	return b;
}

void byte_list_free(byte_list *b) {
	if (b) {
		if (b->list) free(b->list);
		free(b);
	}
}
#pragma endregion

void assemble_file(char *filename) {
	FILE* file = fopen(filename, "r");
	if (!file) return -1;

	// Initialize data structures
	hashmap* named_addresses = hashmap_init(50);
	byte_list* bytes = byte_list_init(256);

	

	hashmap_free(named_addresses);
	fclose(file);
}