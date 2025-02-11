/* Implementation of memory storage for KSU Micro VM.
* Author: Matthew Watson
*/

#include <stdio.h>

#include "leakcheck_util.h"

#include "kvm_memory.h"

kvm_memory* kvm_memory_init(size_t size, uint8_t init_data) {
	kvm_memory* mem = malloc(sizeof(kvm_memory));
	mem->size = size;

	mem->data = malloc(size);

	uint8_t* dat = mem->data;
	for (int i = 0; i < size; i++) {
		dat[i] = init_data;
	}

	return mem;
}

void kvm_memory_free(kvm_memory *mem) {
	if (!mem) return;

	if (mem->data) {
		free(mem->data);
	}

	free(mem);
}

uint8_t kvm_memory_get_byte(kvm_memory* mem, size_t index) {
	if (!mem) return 0;

	if (index > 0 && index < mem->size) {
		return mem->data[index];
	}
	else {
		return 0;
	}
}

void kvm_memory_print_hexdump(kvm_memory* mem, uint16_t start_point, uint16_t length, int print_width) {
	if (start_point >= mem->size || start_point + length >= mem->size) {
		fprintf(stderr, "Error with memory hexdump. Requested bounds [%d, %d] are out of bounds for memory size %d.\n", start_point, start_point+length, (int)mem->size);
	}

	int count = -1;

	printf("     |");
	for (int i = 0; i < print_width; i++) {
		printf(" %02x", i);
	}
	printf("\n______");
	for (int i = 0; i < print_width; i++) {
		printf("___");
	}

	for (uint16_t index = start_point; index < start_point + length; index++) {
		
		count++;
		if (count % print_width == 0) {
			count = 0;
			printf("\n%04x |", index/print_width);
		}

		printf(" %02x", kvm_memory_get_byte(mem, index));
	}
}