/*	Implementation of input-gathering functions.
	Author: Matthew Watson
*/


#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "kvm_input.h"
#include "kvm_memory.h"
#include "leakcheck_util.h"

#include "kvm_mem_map_constants.h"

SDL_Scancode keys[] = {
	SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9,

	SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_J,
	SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
	SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y, SDL_SCANCODE_Z,

	SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_SPACE,

	SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_SEMICOLON,
	SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_GRAVE, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,

	SDL_SCANCODE_ESCAPE, SDL_SCANCODE_HOME, SDL_SCANCODE_END, SDL_SCANCODE_DELETE, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN,

	SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5, SDL_SCANCODE_F6,
	SDL_SCANCODE_F7, SDL_SCANCODE_F8,SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,

	SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT,
	SDL_SCANCODE_RETURN, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB, SDL_SCANCODE_CAPSLOCK
};

void kvm_input_get_keyboard(kvm_memory* mem) {
	if (!mem) return;
	
	SDL_PumpEvents();
	const uint8_t* keystates = SDL_GetKeyboardState(NULL);

	size_t numkeys = sizeof(keys) / sizeof(SDL_KeyCode);
	
	// Create a pointer to the proper location of IO memory for the keyboard
	uint8_t* keys_in_mem = mem->data + IO_MEM_KEYBOARD_LOC;
	if (IO_MEM_KEYBOARD_LOC + numkeys > mem->size) {
		// Error, overflow
		printf("IO Error, Memory overflow with keyboard location %d.\n", IO_MEM_KEYBOARD_LOC);
		return;
	}

	for (size_t i = 0; i < numkeys; i++) {
		if (keystates[keys[i]]) {
			// Key is now pressed

			if (keys_in_mem[i] <= 1) {
				// Key not pressed before
				keys_in_mem[i] = (uint8_t)keydown;
			}
			else {
				// Key pressed before
				keys_in_mem[i] = (uint8_t)keypressed;
			}
		}
		else {
			// Key is now not pressed
			if (keys_in_mem[i] <= 1) {
				// Key not pressed before
				keys_in_mem[i] = (uint8_t)keyidle;
			}
			else {
				// Key just released
				keys_in_mem[i] = (uint8_t)keyup;
			}
		}
	}
}

void kvm_input_get_mouse(kvm_memory* mem) {
	if (!mem) return;

	uint8_t* mouse_mem_loc = mem->data + IO_MEM_MOUSE_LOC;

	int x, y;
	uint32_t mouseState = SDL_GetMouseState(&x, &y);
	//printf("Mouse state: %x. x: %x y: %x\n", mouseState & 0xFF, x&0xff, y&0xff);

	mouse_mem_loc[0] = (uint8_t)x & 0xff;
	mouse_mem_loc[1] = (uint8_t)y & 0xff;
	mouse_mem_loc[2] = (uint8_t)mouseState & 0xff;
}