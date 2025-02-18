/*	File which exists to test the cpu functionality of the KSU Micro.
*	Author: Matthew Watson
*/

#include <stdio.h>

#include "leakcheck_util.h"

#include "kvm_cpu.h"
#include "kvm_memory.h"

void quit(void);

int main(int argc, char* argv[]) {
	
	kvm_memory* mem = kvm_memory_init(0x1000, 0);
	kvm_cpu* cpu = kvm_cpu_init();

	if (mem && cpu && cpu->current_instruction) {
		printf("Success?\nInstruction size: %d\nMemory size: %llu\n\n", (int)cpu->current_instruction->instruction_size, (unsigned long long)mem->size);
	}
	else {
		fprintf(stderr, "Error initializing memory, cpu, or current instruction.");
		return -1;
	}

	print_allocation_data();

	//kvm_cpu_decode_instr(cpu, 0x00); // Should be NOP
	//kvm_cpu_decode_instr(cpu, 0xE9); // JMP absolute?
	//kvm_cpu_decode_instr(cpu, 0x2F); // ROR implicit

	//// Here we go.
	//for (uint8_t i = 1; i != 0; i++) {
	//	//printf("\nOpcode %x\n", i);
	//	kvm_cpu_decode_instr(cpu, i);
	//}
	

	// Load in test programs to test cpu functionality
	
									// LDA #01, STA $00, etc
	//uint8_t instruction_bytes[] = { 0xD0,0x01,0x54,0x00, 0xD0, 0x02, 0x54, 0x01, 0xD0, 0x03, 0x54, 0x02};

	// transfers
	//uint8_t instruction_bytes[] = { 0xD0,0xFF, 0x04, 0x05, 0x54, 0x00, 0x55, 0x01, 0x56, 0x02};
	
	// stack operations
	// uint8_t instruction_bytes[] = { 0xD0,0xFF,0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0xD0, 0x55, 0x0A};

	// Test add/subtract with carry and overflow.
	// make several additions and subtractions, and push the results and processor statuses onto stack.
	
	// uint8_t instruction_bytes[] = {0x0F, 0xD0, 0x01, 0xC4, 0x01, 0x0A, 0x0C, 0x0F, 0xD0, 0x01, 0xC4,0xFF, 0x0A, 0x0C, 0x0F, 0xD0, 0x7F, 0xC4,0x01, 0x0A, 0x0C };
	// subtraction test
	uint8_t instruction_bytes[] = { 0x0E, 0xD0, 0x00, 0xC5, 0x01, 0x0A, 0x0C, 0x0E, 0xD0, 0x80, 0xC5,0x01, 0x0A, 0x0C, 0x0E, 0xD0, 0x7F, 0xC5,0xFF, 0x0A, 0x0C };
	
	printf("\ninstructions size: %d\n", (int)sizeof(instruction_bytes));
	for (int i = 0; i < sizeof(instruction_bytes); i++) {
		mem->data[i + PROGRAM_COUNTER_ENTRY_POINT] = instruction_bytes[i];
	}
	
	for (int i = 0; i < 15; i++) {
		kvm_cpu_cycle(cpu, mem);
		printf("\n\n");
		kvm_cpu_print_status(cpu);
		printf("\n0: %x\n1: %x\n", mem->data[0], mem->data[1]);
	}

	kvm_memory_print_hexdump(mem, 0, 1024); 
	printf("\n\n");
	kvm_cpu_print_status(cpu);

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