/*	Implementation of input-gathering functions.
	Author: Matthew Watson
*/


#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "kvm_input.h"
#include "leakcheck_util.h"

SDL_KeyCode keys[] = {
	SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,

	SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
	SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
	SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,

	SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_SPACE,

	SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH, SDLK_BACKSLASH, SDLK_SEMICOLON,
	SDLK_QUOTE, SDLK_BACKQUOTE, SDLK_MINUS, SDLK_EQUALS, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET,

	SDLK_ESCAPE, SDLK_HOME, SDLK_END, SDLK_DELETE, SDLK_PAGEUP, SDLK_PAGEDOWN,

	SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
	SDLK_F7, SDLK_F8,SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12,

	SDLK_LSHIFT, SDLK_RSHIFT, SDLK_LCTRL, SDLK_RCTRL, SDLK_LALT, SDLK_RALT,
	SDLK_RETURN, SDLK_BACKSPACE, SDLK_TAB, SDLK_CAPSLOCK
};

void kvm_input_get_keyboard(kvm_memory* mem) {
	if (!mem) return;

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
	printf("Mouse state: %x. x: %x y: %x\n", mouseState & 0xFF, x&0xff, y&0xff);

	mouse_mem_loc[0] = (uint8_t)x & 0xff;
	mouse_mem_loc[1] = (uint8_t)y & 0xff;
	mouse_mem_loc[2] = (uint8_t)mouseState & 0xff;
}