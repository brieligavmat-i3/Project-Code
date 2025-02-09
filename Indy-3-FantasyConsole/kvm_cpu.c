/*	Implementation of the KSU Micro VM CPU
*	Author: Matthew Watson
*/

#include "kvm_cpu.h"

uint8_t kvm_cpu_fetch_byte(kvm_memory* mem, size_t index) {
	return kvm_memory_get_byte(mem, index);
}

void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem) {
	/*
	Fetch byte at program counter for opcode,
	Decode opcode 
	Based on instruction type, fetch the next 0, 1, or 2 bytes, and increment the program counter that much.
	Execute the proper instruction.
	*/
}