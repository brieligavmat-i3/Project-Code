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

void byte_list_add(byte_list* b, uint8_t new_byte) {
	if (!b) return;

	if (b->index >= b->size) {
		// resize the list.
		size_t new_size = b->size * 2;
		b->list = realloc(b->list, new_size);
		b->size = new_size;
	}

	b->list[b->index] = new_byte;
	b->index++;
}

void byte_list_print(byte_list* b) {
	for (size_t i = 0; i < b->index; i++) {
		printf("%x, ", b->list[i]);
	}
}

void byte_list_free(byte_list *b) {
	if (b) {
		if (b->list) free(b->list);
		free(b);
	}
}
#pragma endregion

void assemble_file(char *input_filename, char *output_filename) {
	FILE* file = fopen(input_filename, "r");
	if (!file) {
		/*file = fopen(input_filename, "w");
		fprintf(file, "Hey, there!\n");
		fclose(file);*/
		return;
	}

	// Initialize data structures
	hashmap* named_addresses = hashmap_init(50);
	byte_list* bytes = byte_list_init(2);

	byte_list_add(bytes, 0xFF);
	byte_list_add(bytes, 0x01);
	byte_list_add(bytes, 0xFF);
	byte_list_add(bytes, 0x01);

	byte_list_print(bytes);
	
	byte_list_free(bytes);
	hashmap_free(named_addresses);
	fclose(file);
}