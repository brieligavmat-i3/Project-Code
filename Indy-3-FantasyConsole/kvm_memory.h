/* Header for functions related to memory storage.
* Author: Matthew Watson
*/

#pragma once

#include <stdint.h>

typedef struct kvm_memory {
	size_t size;
	uint8_t* data;
} kvm_memory;

kvm_memory* kvm_memory_init(size_t size, uint8_t init_data);
void kvm_memory_free(kvm_memory* mem);

uint8_t kvm_memory_get_byte(kvm_memory* mem, size_t index);

void kvm_memory_print_hexdump(kvm_memory* mem, uint16_t start_point, uint16_t length, int print_width);
