/*	Header for input-gathering functions.
	Author: Matthew Watson
*/

#pragma once

#include "kvm_memory.h"

typedef enum kvm_input_keystate {
	/*
	* Keyidle: key is not pressed and was also not pressed last update
	* Keyup: key is not pressed but was pressed the last update
	* Keydown: key is pressed and wasn't pressed the last update
	* Keypressed: key is pressed and was pressed during the last update
	*/
	keyidle, keyup, keydown, keypressed
}kvm_input_keystate;

void kvm_input_get_keyboard(kvm_memory* mem);
void kvm_input_get_mouse(kvm_memory* mem);
