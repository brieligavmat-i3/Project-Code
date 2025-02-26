/*	File which exists to test the cpu functionality of the KSU Micro.
*	Author: Matthew Watson
*/

#include <stdio.h>
#include <string.h>

#include "leakcheck_util.h"

#include "kvm_cpu.h"
#include "kvm_memory.h"

#define KVM_CPU_TESTING 1

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

	//print_allocation_data();

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
	//uint8_t instruction_bytes[] = { 0x0E, 0xD0, 0x00, 0xC5, 0x01, 0x0A, 0x0C, 0x0E, 0xD0, 0x80, 0xC5,0x01, 0x0A, 0x0C, 0x0E, 0xD0, 0x7F, 0xC5,0xFF, 0x0A, 0x0C };
	
	// Test relative branches and absolute jumps
	//uint8_t instruction_bytes[] = { 0xe9, 0x0a, 0x02, 0xc4, 0x10, 0x0a, 0x0c, 0xd8, 0x05, 0x00, 0xd0, 0xff, 0x0a, 0xdd, 0xf4 };

	//uint8_t instruction_bytes[] = { 0xe9, 0x0a, 0x02, 0xc4, 0x10, 0x0a, 0x0c, 0xd8, 0x05, 0x00, 0xd0, 0xff, 0x0a, 0xdd, 0xf4 };


	/*printf("\ninstructions size: %d\n", (int)sizeof(instruction_bytes));
	for (int i = 0; i < sizeof(instruction_bytes); i++) {
		mem->data[i + PROGRAM_COUNTER_ENTRY_POINT] = instruction_bytes[i];
	}*/

	printf("Files:\n");
	system("dir tests /B");

	printf("Enter filename to assemble and run (omit '.txt'): ");
	char fname[50];

	scanf("%s", fname);
	printf("\nFilename: %s\n", fname);

	char sys_string[100] = "python assembler.py ";
	strcat(sys_string, fname);
	strcat(sys_string, ".txt");
	if (strnlen(sys_string, 100) == 100) {
		printf("Error with strings.\n");
		return -1;
	}

	//printf("sys string: %s\n", sys_string);

	// Run the python script.
	if(system(sys_string)) return -1;
	//printf("execution result: %d\n", result);

	char out_file_name[100] = "outs/";
	strcat(out_file_name, fname);
	strcat(out_file_name, ".kvmbin");

	printf("binary file name: %s\n", out_file_name);

	FILE* code_file = fopen(out_file_name, "rb"); // open the file to read bytes
	if (!code_file) {
		printf("Error opening assembled file.\n");
		return -1;
	}

	// Get the file size (i.e. number of bytes to copy over to the array)
	fseek(code_file, 0L, SEEK_END);
	size_t file_size = ftell(code_file);
	printf("Code file size: %d bytes.\n", file_size);
	rewind(code_file);

	// Copy the data into memory
	// TODO: add bounds checking
	fread(mem->data + PROGRAM_COUNTER_ENTRY_POINT, 1, file_size, code_file);
	fclose(code_file);

	
	/*
	for (int i = 0; i < 10; i++) {
		kvm_cpu_cycle(cpu, mem);
		printf("\n\n");
		kvm_cpu_print_status(cpu);
		printf("\n0: %x\n1: %x\n", mem->data[0], mem->data[1]);
	}
	*/

	// Cpu cycle until system calls happen.
	size_t cycle_count = 0;

	bool cpu_running = true;
	while (cpu_running) {
		kvm_cpu_cycle(cpu, mem);

		if (mem->data[0] > 0) {
			switch (mem->data[0]) {
			case 1: // Quit
				cpu_running = false; break;
			case 2: // print cpu data
				printf("\n");
				kvm_cpu_print_status(cpu);
				mem->data[0] = 0;
				break;
			}
		}

		if (++cycle_count > 10000) {
			cpu_running = false;
			printf("Error, ten thousand cycles reached.\n");
		}
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