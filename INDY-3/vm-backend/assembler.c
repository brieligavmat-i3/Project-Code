/*	Implementation file for assembler.h
	Author: Matthew Watson
*/

#include <stdio.h>
#include <string.h>

#include "assembler.h"

#include "str_uint16_hash.h"
#include "linklist.h"

#include "leakcheck_util.h"

// For kvm_instruction and related enums.
#include "kvm_cpu.h"

#pragma region Byte List
byte_list* byte_list_init(size_t init_size) {
	byte_list* b = malloc(sizeof(byte_list));
	b->size = init_size;
	b->list = malloc(init_size);
	b->index = 0;

	return b;
}

void byte_list_add(byte_list* b, uint8_t new_byte) {
	if (!b) return;

	if (b->index >= b->size) {
		// resize the list.
		size_t new_size = b->size * 2;
		b->list = realloc(b->list, new_size);
		b->size = new_size;
	}

	b->list[b->index] = new_byte;
	b->index++;
}

void byte_list_print(byte_list* b) {
	for (size_t i = 0; i < b->index; i++) {
		printf("%x, ", b->list[i]);
	}
}

void byte_list_free(byte_list *b) {
	if (b) {
		if (b->list) free(b->list);
		free(b);
	}
}
#pragma endregion

#pragma region Process Line

/* Steps for processing line:
* 
*  Check for named address, if so, add to hashmap
*  
*  Check first three letters for the thing 
*/

int get_index_next_non_space(char* line, size_t start_index, size_t line_length) {
	size_t ind = start_index;
	if (ind >= line_length) return -1;
	
	while (line[ind] == ' ' || line[ind] == '\t');
	{
		if (ind >= line_length) return -1;
		ind++;
	} 
	
	return ind;

}

void make_lowercase(char* str, size_t length) {
	for (int i = 0; i < length; i++) {
		if (str[i] >= 0x41 && str[i] <= 0x5a) {
			str[i] += 0x20;
		}
	}
}

void str_copy_until_nonchar(char* out_destination, char* source, size_t max_length) {
	size_t index = 0;

	bool valid_char = true;
	while (valid_char && index < max_length-1) {
		valid_char = false;

		if (source[index] >= 0x30 && source[index] <= 0x39) {
			// Numbers
			valid_char = true;
		}

		if (source[index] >= 0x41 && source[index] <= 0x5a) {
			// Uppercase letters
			valid_char = true;
		}

		if (source[index] >= 0x61 && source[index] <= 0x7a) {
			// Lowercase letters
			valid_char = true;
		}

		if (source[index] == '_') valid_char = true;

		if(valid_char) out_destination[index] = source[index];

		index++;
	}
	out_destination[index] = '\0';
}

uint16_t first_pass_index = 0;
void process_line_first_pass(char* line, hashmap* named_addresses, byte_list* bytes) {
	size_t line_len = strlen(line);

	if (line_len <= 1) return;

	int ind = get_index_next_non_space(line, 0, line_len);

	if (line[ind] == '.') {
		// This is a named address
		char name[50];

		// Omit the '.'
		str_copy_until_nonchar(name, line + ind + 1, sizeof(name));
		hashmap_insert_kvp(named_addresses, name, first_pass_index);
	}
	else if (line[ind] == ';') {
		// Comment
		return;
	}
	else{
		// Expecting an instruction
		char instr[3];
		int size = sizeof(instr);

		str_copy_until_nonchar(instr, line + ind, size);
		make_lowercase(instr, size);

		uint32_t int_representation = (instr[0] << 16) + (instr[1] << 8) + instr[2];

		if (int_representation == 0x646174) {
			// dat
			printf("DAT\n");
			return;
		}

		switch (int_representation) {
		case 0x6e6f70: // nop
			printf("NOP\n");
			break;
		case 0x62726b: // brk
			printf("BRK\n");
			break;
		}
	}
}

#pragma endregion

void assemble_file(char *input_filename, char *output_filename) {
	FILE* in_file = fopen(input_filename, "r");
	if (!in_file) return;

	// Initialize data structures
	hashmap* named_addresses = hashmap_init(50);
	byte_list* bytes = byte_list_init(2);

	char line_buffer[256];

	// First pass
	while (fgets(line_buffer, sizeof(line_buffer), in_file)) {
		process_line_first_pass(line_buffer, named_addresses, bytes);
	}
	/*
	FILE* out_file = fopen(output_filename, "w");

	if (out_file) {

		for (size_t i = 0; i < bytes->index; i++) {
			fputc(bytes->list[i], out_file);
		}
	}*/
	
	byte_list_free(bytes);
	hashmap_free(named_addresses);
	fclose(in_file);
}