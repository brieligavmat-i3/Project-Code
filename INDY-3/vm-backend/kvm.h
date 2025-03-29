/* Header file for the virtual machine driver
*  Author: Matthew Watson
*/

#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <SDL.h>


// Call this first
int kvm_init(void);

// Call this second with the filename of the currently loaded assembly file.
// Runs the assembler python script, and loads the resulting file into ROM
int kvm_load_instructions(const char* filename);

// Call this third.
// Run the VM. If max_cycles is > 0, The CPU will force quit after that many cycles (set it to zero to ignore this).
// max_cycles can also be set by a system call. (id 4)
int kvm_start(int max_cycles);

// Call this last, after kvm_start() returns
int kvm_quit(void);

void kvm_hexdump(int start_page, int page_count, bool print_cpu_status);

uint8_t* kvm_get_memory_pointer(void);
SDL_Surface* kvm_get_display_surface(void);

