/*
* Author: Matthew Watson
*/

#pragma once

#include <stdint.h> // For guranteed fixed-width integers.

#include "kvm_memory.h"
/*

Hm.

fetch
decode
execute

registers:
a, x, y, stack, PC

*/

typedef struct kvm_cpu {
	uint16_t program_counter;

	uint8_t accumulator;
	uint8_t x_index;
	uint8_t y_index;

	uint8_t stack_ptr;
} kvm_cpu;

uint8_t kvm_cpu_fetch_byte(kvm_memory* mem, size_t index);


/* Perform fetch, decode, and execute operations.
*/
void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem);

