/*	Implementation of the KSU Micro VM CPU
*	Author: Matthew Watson
*/

#include <stdio.h>
#include <stdlib.h>

#include "leakcheck_util.h"

#include "kvm_cpu.h"

static uint8_t extract_bits(uint8_t target, uint8_t mask, uint8_t shift) {
	return (target & mask) >> shift;
}

uint8_t kvm_cpu_fetch_byte(kvm_memory* mem, size_t index) {
	return kvm_memory_get_byte(mem, index);
}

#pragma region Helper functions for decoding.

static void instr_reset_defaults(kvm_instruction* out_instr) {
	out_instr->instruction_size = kvms_invalid;
	out_instr->addressing_mode = kvma_invalid;
	out_instr->instruction_class = kvmc_invalid;
	out_instr->register_operand = kvmr_invalid;

	out_instr->byte1 = 0;
	out_instr->byte2 = 0;
}

// Call this first.
static void decode_instr_size(uint8_t instruction, kvm_instruction* out_instr) {
	uint8_t pattern = extract_bits(instruction, 0b11000000, 6);
	switch (pattern) {
	case 0b00:			// 00******
		out_instr->instruction_size = kvms_small;
		break;
	case 0b01:			// 01******
		out_instr->instruction_size = kvms_med;
		break;
	case 0b10:			// 10******
		out_instr->instruction_size = kvms_large;
		break;
	case 0b11:			// 11******
	{
		pattern = extract_bits(instruction, 0b00100000, 5);
		if (pattern == 0b01) {	// 111*****
			out_instr->instruction_size = kvms_large;
		}
		else {					// 110*****
			out_instr->instruction_size = kvms_med;
		}
	}
	break;
	default:
		fprintf(stderr, "Error with kvm_cpu.c, decode_instr_size(): Pattern \"%x\" is not matched.", pattern);
		break;
	}
}

// Call this AFTER decode_instr_size(), when out_instr should contain the size.
static void decode_instr_addressing_mode(uint8_t instruction, kvm_instruction* out_instr) {
	uint8_t pattern = 0;
	
	switch (out_instr->instruction_size) {
	case kvms_small:
		out_instr->addressing_mode = kvma_implicit; // The only one-byte addressing mode.
		break;
	case kvms_med:
	{
		pattern = extract_bits(instruction, 0b11100000, 5);

		switch (pattern) {
		case 0b010:	// Standard zero-page.
			out_instr->addressing_mode = kvma_zeropage;
			break;
		case 0b011: // Zero page, x (or y)
		{
			pattern = extract_bits(instruction, 0b00011111, 0);
			switch (pattern) {
			case 0b10001:	// LDX $,y
				out_instr->addressing_mode = kvma_zpy;
				break;
			case 0b10101:	// STX $,y
				out_instr->addressing_mode = kvma_zpy;
				break;
			default:		// All others are z.p., x.
				out_instr->addressing_mode = kvma_zpx;
				break;
			}
		}
		break;
		case 0b110:
			// 0v11011000 is the lowest opcode representing a relative addr. mode in this category.
			if (instruction < 0b11011000) {
				out_instr->addressing_mode = kvma_immediate;
			}
			else {
				out_instr->addressing_mode = kvma_relative;
			}
			break;
		default:
			fprintf(stderr, "Error with kvm_cpu.c, decode_instr_addressing_mode(): Bit mask 0b11100000 \"%x\" is not matched.", pattern);
			break;
		}
		break;
	}
	case kvms_large:
	{
		pattern = extract_bits(instruction, 0b11100000, 5);
		if (pattern == 0b111) {	// Absolute addressing
			out_instr->addressing_mode = kvma_absolute;
		}else{ //0b10****** opcodes
			pattern = extract_bits(instruction, 0b00110000, 4);
			if (pattern == 0b10) { // x and y indirects
				pattern = extract_bits(instruction, 0b00001000, 3);
				if (pattern == 0b1) {	// indirect indexed, y
					out_instr->addressing_mode = kvma_yind;
				}
				else {					// indexed indirect, x
					out_instr->addressing_mode = kvma_indx;
				}
			}
			else {
				pattern = extract_bits(instruction, 0b00111111, 0);
				if (pattern > 0b110100 || pattern == 0b010001 || pattern == 0b010011) { // absolute, y
					out_instr->addressing_mode = kvma_aby;
				}
				else if(pattern == 0b001001){ // JMP instruction, indirect addr. mode.
					out_instr->addressing_mode = kvma_indirect;
				}
				else {
					out_instr->addressing_mode = kvma_abx;
				}
			}
		}
	}
		break;
	default:
		fprintf(stderr, "Error with kvm_cpu.c, decode_instr_addressing_mode(): Instruction size \"%d\" is not matched.", out_instr->instruction_size);
		break;
	}
}

#pragma endregion

void kvm_cpu_decode_instr(uint8_t instruction, kvm_instruction* out_instr){
	
	instr_reset_defaults(out_instr);

	decode_instr_size(instruction, out_instr);
	decode_instr_addressing_mode(instruction, out_instr);
}


void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem) {
	uint16_t pc = cpu->program_counter;

	uint8_t current_opcode = kvm_cpu_fetch_byte(mem, pc++);

	kvm_cpu_decode_instr(current_opcode, cpu->current_instruction);
	/*
	Fetch byte at program counter for opcode,
	Decode opcode 
	Based on instruction type, fetch the next 0, 1, or 2 bytes, and increment the program counter that much.
	Execute the proper instruction.
	*/
}

kvm_cpu* kvm_cpu_init(void) {
	kvm_cpu* cpu = malloc(sizeof(kvm_cpu));
	cpu->accumulator = 0;
	cpu->x_index = 0;
	cpu->y_index = 0;

	cpu->stack_ptr = 0;

	cpu->program_counter = PROGRAM_COUNTER_ENTRY_POINT;

	cpu->processor_status = DEFAULT_PROCESSOR_STATUS;

	kvm_instruction* instruction = malloc(sizeof(kvm_instruction));
	instr_reset_defaults(instruction);
}

void kvm_cpu_free(kvm_cpu* cpu) {
	if (cpu) {
		if (cpu->current_instruction) free(cpu->current_instruction);
		free(cpu);
	}
}