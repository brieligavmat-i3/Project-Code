/*	Imiplementation of the virtual machine driver
	Author: Matthew Watson
*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "kvm.h"
#include "leakcheck_util.h"

#include "kvm_memory.h"
#include "kvm_cpu.h"

#include "kvm_input.h"
#include "kvm_gpu.h"

#include "kvm_graphicsloader.h"

#include "kvm_mem_map_constants.h"

// SDL Includes
#include <SDL.h>

// System calls
#define SYSCALL_QUIT 1
#define SYSCALL_PRINTCPU 255
#define SYSCALL_PRINTF 254

#define SYSCALL_LOAD_PALETTES 2
#define SYSCALL_LOAD_GRAPHICS 3
#define SYSCALL_SET_CYCLE_MAX 4

#define SYSCALL_SET_TIMER 10
#define SYSCALL_START_TIMER 11
#define SYSCALL_STOP_TIMER 12
#define SYSCALL_GET_TIMER 13
#define SYSCALL_DELAY 14

#define SYSCALL_GET_KEY_INPUT 50
#define SYSCALL_GET_MOUSE_INPUT 51
#define SYSCALL_GET_CONTROLLER_INPUT 52

#define SYSCALL_GPU_REFRESH 100

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

	kvm_gpu_init(mem);
	
	return 0;
}

static size_t file_get_size(FILE* file) {
	// Get the file size (i.e. number of bytes to copy over to the array)
	fseek(file, 0L, SEEK_END);
	size_t file_size = ftell(file);
	rewind(file);

	return file_size;
}

// Takes a binary file of bytes and puts them in a specififed memory location
static int load_binary_file_to_memory(const char* filename, size_t offset) {
	FILE* code_file = fopen(filename, "rb"); // open the file to read bytes
	if (!code_file) {
		printf("Error opening binary file.\n");
		return -1;
	}

	size_t file_size = file_get_size(code_file);

	// Copy the data into memory
	if (file_size > mem->size - offset) {
		printf("Error loading ROM, file size of %d exceeds memory capacity %d.\n", (int)file_size, (int)(mem->size - offset));
		return -1;
	}

	fread(mem->data + offset, 1, file_size, code_file);
	fclose(code_file);

	return 0;
}

int kvm_load_instructions(const char* filename) {
	if (!cpu || !mem) return -1;

	char sys_string[100] = "python assembler.py ";

	char prog_count_str[20];
	sprintf(prog_count_str, "%d", INSTRUCTION_ROM_MEM_LOC);

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

	int load_result = load_binary_file_to_memory(out_file_name, INSTRUCTION_ROM_MEM_LOC);
	if (load_result != 0) return -1;

	return 0;
}

// Runs the python scripts to load in the graphics, then puts the data in their proper ROM spots.
int kvm_load_graphics(const char* tile_filename, const char* palette_filename) {
	if (!cpu || !mem) return -1;

	char sys_string[100] = "python graphics_loader.py ";

	char t_fname[50] = "-1";
	if (tile_filename) {
		strncpy(t_fname, tile_filename, 50);
		t_fname[49] = 0;
	}

	char p_fname[50] = "-1";
	if (palette_filename) {
		strncpy(p_fname, palette_filename, 50);
		p_fname[49] = 0;
	}

	printf("t: %s\np: %s\n", t_fname, p_fname);

	strcat(sys_string, t_fname);
	strcat(sys_string, " ");
	strcat(sys_string, p_fname);
	if (strnlen(sys_string, 100) == 100) {
		printf("Error with strings.\n");
		return -1;
	}

	// Run the python script.
	if (system(sys_string)) return -1;

	if (tile_filename) {
		char out_tile_filename[100] = "graphics/out/";
		strcat(out_tile_filename, tile_filename);
		strcat(out_tile_filename, ".kvmpix");

		printf("binary file name: %s\n", out_tile_filename);

		if (load_binary_file_to_memory(out_tile_filename, GRAPHICS_ROM_MEM_LOC) != 0) return -1;
	}
	
	if (palette_filename) {
		char out_palette_filename[100] = "graphics/out/";
		strcat(out_palette_filename, palette_filename);
		strcat(out_palette_filename, ".kvmpal");

		if (load_binary_file_to_memory(out_palette_filename, VRAM_COLOR_PALETTES) != 0) return -1;
	}
	

	return 0;
}

// Gets a character string from KVM memory.
static uint16_t load_string(char* str, uint16_t start_location, uint16_t max_len) {
	bool null_terminated = false;

	uint16_t end_point = start_location + max_len;

	for (uint16_t i = start_location; i < start_location + max_len; i++) {
		uint8_t byte = kvm_memory_get_byte(mem, i);
		str[i - start_location] = byte;

		//printf("%d:'%c' ", i, byte);

		if (byte == '\0') {
			null_terminated = true;
			end_point = i+1;
			break;
		}
	}

	if (!null_terminated) str[max_len - 1] = '\0';
	//printf("\nLoaded string from memory: %s\n", str);

	return end_point;
}

int kvm_start(int max_cycles) {
	if (!cpu || !mem) return -1;

	// Cpu cycle until system calls happen.
	size_t cycle_count = 0;

	#pragma region Timer Variables
	uint64_t sdl_timer_start_time = 0;
	uint64_t sdl_timer_current_time = 0;
	uint16_t kvm_timer = 0;
	#pragma endregion

	bool cpu_running = true;
	while (cpu_running) {
		kvm_cpu_cycle(cpu, mem);

		// Test for system calls.
		if (mem->data[0] > 0) {

			// Get the address from the second two bytes of memory, right after the syscall byte.
			uint16_t syscall_addr = mem->data[1] | ((uint16_t)mem->data[2] << 8);

			switch (mem->data[0]) {
			case SYSCALL_QUIT: // Quit
				cpu_running = false; break;
			case SYSCALL_PRINTCPU: // print cpu data
				printf("\n");
				kvm_cpu_print_status(cpu);
				break;
			case SYSCALL_PRINTF:
			{
				char* print_string = malloc(256);
				uint16_t str_end_pt = load_string(print_string, syscall_addr, 256);
				uint8_t bytes_to_print = kvm_memory_get_byte(mem, str_end_pt);
				printf("addr: %x endpt: %x bytes: %x\n",syscall_addr,  str_end_pt, bytes_to_print);
				printf("%s ", print_string);

				for (int i = 0; i < bytes_to_print; i++) {
					printf("%x ", kvm_memory_get_byte(mem, str_end_pt + i + 1));
				}
				printf("\n");

				free(print_string);
				break;
			}
			case SYSCALL_LOAD_PALETTES:
			{
				char* graphics_fname = malloc(FILENAME_LENGTH);
				load_string(graphics_fname, syscall_addr, FILENAME_LENGTH);

				char* binary_fname = load_graphics(graphics_fname, true);
				if (!binary_fname || load_binary_file_to_memory(binary_fname, VRAM_COLOR_PALETTES) == -1) {
					printf("Failed to load graphics file %s.\n", graphics_fname);
					mem->data[1] = 0xFF; // FF for "Freakin' Failure"
				}
				else {
					mem->data[1] = 0x00; // 0 for success.
				}

				free(binary_fname);
				free(graphics_fname);
			}
			break;
			case SYSCALL_LOAD_GRAPHICS:
			{
				char* graphics_fname = malloc(FILENAME_LENGTH);
				load_string(graphics_fname, syscall_addr, FILENAME_LENGTH);

				char* binary_fname = load_graphics(graphics_fname, false);
				if (!binary_fname || load_binary_file_to_memory(binary_fname, GRAPHICS_ROM_MEM_LOC) == -1) {
					printf("Failed to load graphics file %s.\n", graphics_fname);
					mem->data[1] = 0xFF; // FF for "Freakin' Failure"
				}
				else {
					mem->data[1] = 0x00; // 0 for success.
				}

				free(binary_fname);
				free(graphics_fname);
			}
			break;
			case SYSCALL_SET_CYCLE_MAX:
				// Set the max number of cpu cycles to go, using the uint16 stored in the second two bytes of memory.
				max_cycles = syscall_addr;
				break;

				// Timer interaction
			case SYSCALL_START_TIMER:
				kvm_timer = 0;
				sdl_timer_start_time = SDL_GetTicks64();
				break;
			case SYSCALL_STOP_TIMER:
				sdl_timer_current_time = SDL_GetTicks64() - sdl_timer_start_time;
				break;
			case SYSCALL_GET_TIMER:
			{
				sdl_timer_current_time = SDL_GetTicks64() - sdl_timer_start_time;
				kvm_timer = (uint16_t)(sdl_timer_current_time % 0xFFFF);
				mem->data[1] = (uint8_t)(kvm_timer & 0xFF);
				mem->data[2] = (uint8_t)(kvm_timer >> 8) & 0xFF;
			}
				break;
			case SYSCALL_DELAY:
				SDL_Delay(syscall_addr);
				break;

			// Input
			case SYSCALL_GET_KEY_INPUT:
				kvm_input_get_keyboard(mem);
				break;
			case SYSCALL_GET_MOUSE_INPUT:
				kvm_input_get_mouse(mem);
				break;
			case SYSCALL_GPU_REFRESH:
				kvm_gpu_refresh_graphics(mem);
				break;
			}

			mem->data[0] = 0;
		}

		if (max_cycles > 0 && ++cycle_count > max_cycles) {
			cpu_running = false;
			printf("Error, %d cycles reached.\n", max_cycles);
		}
	}

	return 0;
}

int kvm_quit(void) {

	kvm_gpu_quit();

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

uint8_t* kvm_get_memory_pointer(void) {
	if (!mem) return NULL;
	if (!mem->data) return NULL;

	return mem->data;
}

SDL_Surface* kvm_get_display_surface(void) {
	// will do later
	return NULL;
}
