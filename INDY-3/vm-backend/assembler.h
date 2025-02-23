/*	Header file for the code assembler.
*	Author: Matthew Watson
*/

#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct named_address {
	uint16_t low_byte_position;
	char name[50];
}named_address;

typedef struct byte_list {
	uint8_t* list;
	size_t size;
	size_t index;
}byte_list;

void assemble_file(char* input_filename, char* output_filename);

