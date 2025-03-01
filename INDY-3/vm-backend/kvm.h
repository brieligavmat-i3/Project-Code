/* Header file for the virtual machine driver
*  Author: Matthew Watson
*/

#pragma once
#include <stdbool.h>

/*
So, we have to take in a filename to assemble, then assemble, and run it until it's done.
*/

int kvm_init(void);

// Runs the assembler python script, and loads the resulting file into ROM
int kvm_load_instructions(const char* filename);

// Runs the python scripts to load in the graphics, then puts the data in their proper ROM spots.
int kvm_load_graphics(const char* tile_filename, const char* palette_filename);

// Run the VM. If max_cycles is > 0, The CPU will force quit after that many cycles.
int kvm_start(int max_cycles);
int kvm_quit(void);

void kvm_hexdump(int start_page, int page_count, bool print_cpu_status);
