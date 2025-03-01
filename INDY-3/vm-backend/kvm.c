/*	Imiplementation of the virtual machine driver
	Author: Matthew Watson
*/
#include <stdbool.h>
#include <stdio.h>

#include "kvm.h"
#include "leakcheck_util.h"

#include "kvm_memory.h"
#include "kvm_cpu.h"

// SDL Includes
#include <SDL.h>

kvm_memory* mem;
kvm_cpu* cpu;

bool is_running = false;

int kvm_init(void) {
	mem = kvm_memory_init(0xFFFF, 0);
	cpu = kvm_cpu_init();

	if (!cpu || !mem) return -1;
	
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("Error with SDL initialization.\n");
		return -2;
	}
	
	return 0;
}

int kvm_load_instructions(const char* filename) {
	if (!cpu || !mem) return -1;

	char sys_string[100] = "python assembler.py ";

	char prog_count_str[20];
	sprintf(prog_count_str, "%d", PROGRAM_COUNTER_ENTRY_POINT);

	strcat(sys_string, filename);
	strcat(sys_string, ".txt ");
	strcat(sys_string, prog_count_str);
	if (strnlen(sys_string, 100) == 100) {
		printf("Error with strings.\n");
		return -1;
	}

	// Run the python script.
	if (system(sys_string)) return -1;

	char out_file_name[100] = "outs/";
	strcat(out_file_name, filename);
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
	rewind(code_file);

	// Copy the data into memory
	if (file_size > mem->size - PROGRAM_COUNTER_ENTRY_POINT) {
		printf("Error loading ROM, file size of %d exceeds memory capacity %d.\n", (int)file_size, (int)mem->size - PROGRAM_COUNTER_ENTRY_POINT);
		return -1;
	}

	fread(mem->data + PROGRAM_COUNTER_ENTRY_POINT, 1, file_size, code_file);
	fclose(code_file);
}

int kvm_load_graphics(const char* tile_filename, const char* palette_filename) {

}

int kvm_start(int max_cycles) {
	if (!cpu || !mem) return -1;

	// Cpu cycle until system calls happen.
	size_t cycle_count = 0;

	bool cpu_running = true;
	while (cpu_running) {
		kvm_cpu_cycle(cpu, mem);

		// Test for system calls.
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

		if (max_cycles > 0 && ++cycle_count > max_cycles) {
			cpu_running = false;
			printf("Error, %d cycles reached.\n", max_cycles);
		}
	}
}

int kvm_quit(void) {
	kvm_cpu_free(cpu);
	kvm_memory_free(mem);

	is_running = false;

	return 0;
}

void kvm_hexdump(int start_page, int page_count, bool print_cpu_status) {
	if (!mem) return;

	kvm_memory_print_hexdump(mem, start_page * 256, page_count * 256);
	if (print_cpu_status && cpu) {
		printf("\n\nCPU Status:\n");
		kvm_cpu_print_status(cpu);
	}
}
