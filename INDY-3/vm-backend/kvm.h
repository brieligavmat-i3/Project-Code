/* Header file for the virtual machine driver
*  Author: Matthew Watson
*/

#pragma once

/*
So, we have to take in a filename to assemble, then assemble, and run it until it's done.
*/

int kvm_init(void);

int kvm_load_instructions(const char* filename);

int kvm_load_graphics(const char* tile_filename, const char* palette_filename);
