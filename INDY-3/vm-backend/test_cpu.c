/*	File which exists to test the cpu functionality of the KSU Micro.
*	Author: Matthew Watson
*/

#include <stdio.h>
#include <string.h>

#include "leakcheck_util.h"

#include "kvm_cpu.h"
#include "kvm_memory.h"

void quit(void);

int main(int argc, char* argv[]) {
	
	kvm_memory* mem = kvm_memory_init(0x1000, 0);
	kvm_cpu* cpu = kvm_cpu_init();

	if (mem && cpu && cpu->current_instruction) {
		//printf("Success!\nInstruction size: %d\nMemory size: %llu\n\n", (int)cpu->current_instruction->instruction_size, (unsigned long long)mem->size);
	}
	else {
		fprintf(stderr, "Error initializing memory, cpu, or current instruction.");
		return -1;
	}

	

	printf("Files:\n");
	system("dir tests /B");

	printf("Enter filename to assemble and run (omit '.txt'): ");
	char fname[50];

	scanf("%s", fname);
	printf("\nFilename: %s\n", fname);

	char sys_string[100] = "python assembler.py ";

	char prog_count_str[20];
	sprintf(prog_count_str, "%d", PROGRAM_COUNTER_ENTRY_POINT);

	strcat(sys_string, fname);
	strcat(sys_string, ".txt ");
	strcat(sys_string, prog_count_str);
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
			case 0xFF:
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