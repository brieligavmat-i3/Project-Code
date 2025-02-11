/*	File which exists to test the cpu functionality of the KSU Micro.
*	Author: Matthew Watson
*/

#include <stdio.h>

#include "leakcheck_util.h"

#include "kvm_cpu.h"
#include "kvm_memory.h"

void quit(void);

int main(int argc, char* argv[]) {
	
	kvm_memory* mem = kvm_memory_init(0xFFFF, 0);
	kvm_cpu* cpu = kvm_cpu_init();

	if (mem && cpu && cpu->current_instruction) {
		printf("Success?\nInstruction size: %d\nMemory size: %llu\n\n", (int)cpu->current_instruction->instruction_size, (unsigned long long)mem->size);
	}
	print_allocation_data();

	//kvm_cpu_decode_instr(cpu, 0x00); // Should be NOP
	//kvm_cpu_decode_instr(cpu, 0xE9); // JMP absolute?
	//kvm_cpu_decode_instr(cpu, 0x2F); // ROR implicit

	// Here we go.
	for (uint8_t i = 1; i != 0; i++) {
		//printf("\nOpcode %x\n", i);
		kvm_cpu_decode_instr(cpu, i);
	}

	kvm_memory_print_hexdump(mem, 0, 4096, 16);


	kvm_cpu_free(cpu);
	kvm_memory_free(mem);
	quit();

	return 0;
}

void quit(void) {
	printf("\n");
	print_allocation_data();
	clean_allocation();
}