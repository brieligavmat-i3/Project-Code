/* Implementation of memory storage for KSU Micro VM.
* Author: Matthew Watson
*/

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
}