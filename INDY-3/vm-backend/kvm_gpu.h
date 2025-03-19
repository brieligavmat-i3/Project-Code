/*	Header file for the KSU Micro graphics processor
	Author: Matthew Watson
*/

#pragma once
#include "kvm_memory.h"


// Create the SDL window and renderer, and set up the display surface.
int kvm_gpu_init(kvm_memory* mem);

// Tear down the things that were created with kvm_gpu_init()
void kvm_gpu_quit(void);

// Access memory and draw the proper pixels to the screen, then refresh the display.
int kvm_gpu_refresh_graphics(kvm_memory* mem);