/* Header file for the virtual machine driver
*  Author: Matthew Watson
*/

#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <SDL.h>

/*
So, we have to take in a filename to assemble, then assemble, and run it until it's done.
*/

int kvm_init(void);

// Runs the assembler python script, and loads the resulting file into ROM
int kvm_load_instructions(const char* filename);

// Run the VM. If max_cycles is > 0, The CPU will force quit after that many cycles.
// max_cycles can also be set by a system call. (id 4)
int kvm_start(int max_cycles);
int kvm_quit(void);

void kvm_hexdump(int start_page, int page_count, bool print_cpu_status);

uint8_t* kvm_get_memory_pointer(void);
SDL_Surface* kvm_get_display_surface(void);

