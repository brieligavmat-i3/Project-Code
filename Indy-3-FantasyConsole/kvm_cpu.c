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
	out_instr->is_finished_constructing = false;

	out_instr->instruction_size = kvms_invalid;
	out_instr->addressing_mode = kvma_invalid;
	out_instr->instruction_class = kvmc_invalid;
	out_instr->register_operand = kvmr_invalid;

	out_instr->byte1 = 0;
	out_instr->byte2 = 0;
}

static void instr_set(kvm_instruction* out_instr, kvm_instruction_size s, kvm_addressing_mode a, kvm_instruction_class c, kvm_register_operand r) {
	if (s) out_instr->instruction_size	= s;
	if (a) out_instr->addressing_mode	= a;
	if (c) out_instr->instruction_class = c;
	if (r) out_instr->register_operand	= r;
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

static void decode_instr_class(uint8_t instruction, kvm_instruction* out_instr) {
	kvm_instruction_class c = kvmc_invalid;
	kvm_register_operand r = kvmr_invalid;
	kvm_addressing_mode mode = out_instr->addressing_mode;

	if (odd_opcodes_out[instruction]) {
		switch (instruction) {

		// Implicit opcodes, which do their own thing.
		case 0x0:
			r = kvmr_none;
			c = kvmc_no_op;
			break;
		case 0x1: // BRK
			r = kvmr_none;
			c = kvmc_force_interrupt;
			break;
		case 0x2:
		case 0x3: // RTS and RTI
			r = kvmr_none;
			c = kvmc_return;
			break;
		case 0x4: // TAX
			c = kvmc_transfer_accumulator;
			r = kvmr_x_index;
			break;
		case 0x5: // TAY
			c = kvmc_transfer_accumulator;
			r = kvmr_y_index;
			break;
		case 0x6:
			c = kvmc_transfer;
			r = kvmr_x_index;
			break;
		case 0x7:
			c = kvmc_transfer;
			r = kvmr_y_index;
			break;
		case 0x8: // TSX
			c = kvmc_transfer_stack;
			r = kvmr_stack_ptr;
			break;
		case 0x9: // TXS
			c = kvmc_transfer_stack;
			r = kvmr_x_index;
			break;
		case 0xA:
			c = kvmc_stack_push;
			r = kvmr_accumulator;
			break;
		case 0xC:
			c = kvmc_stack_push;
			r = kvmr_processor_status;
			break;
		case 0xB:
			c = kvmc_stack_pull;
			r = kvmr_accumulator;
			break;
		case 0xD:
			c = kvmc_stack_pull;
			r = kvmr_processor_status;
			break;
		case 0xE:
			c = kvmc_set_flag;
			r = kvmr_flag_carry;
			break;
		case 0xF:
			c = kvmc_clear_flag;
			r = kvmr_flag_carry;
			break;
		case 0x10:
			c = kvmc_clear_flag;
			r = kvmr_flag_overflow;
			break;

		// Out of place opcodes
		case 0x89: // JMP Indirect
		case 0xE9: // JMP Absolute
			c = kvmc_jump;
			r = kvmr_none;
			break;
		case 0x93: // Out of place LDA's
		case 0xA3:
		case 0xAB:
			c = kvmc_load;
			r = kvmr_accumulator;
			break;
		case 0xB7: // Out of place STA's
		case 0xA7:
		case 0xAF:
			c = kvmc_store;
			r = kvmr_accumulator;
			break;
		case 0xEB: // JSR Absolute
			c = kvmc_jump_to_subroutine;
			r = kvmr_none;
			break;

		// Out of place arithmetics
		case 0xB8:
			c = kvmc_and;
			r = kvmr_accumulator;
			break;
		case 0xB9:
			c = kvmc_or;
			r = kvmr_accumulator;
			break;
		case 0xBA:
			c = kvmc_xor;
			r = kvmr_accumulator;
			break;
		case 0xBC:
			c = kvmc_add;
			r = kvmr_accumulator;
			break;
		case 0xBD:
			c = kvmc_subtract;
			r = kvmr_accumulator;
			break;
		case 0xBE:
			c = kvmc_compare;
			r = kvmr_accumulator;
			break;
		case 0xA8: // Indirect Indexed, Y operations (They don't fit well)
			c = kvmc_and;
			r = kvmr_accumulator;
			break;
		case 0xA9:
			c = kvmc_or;
			r = kvmr_accumulator;
			break;
		case 0xAA:
			c = kvmc_xor;
			r = kvmr_accumulator;
			break;
		case 0xAC:
			c = kvmc_add;
			r = kvmr_accumulator;
			break;
		case 0xAD:
			c = kvmc_subtract;
			r = kvmr_accumulator;
			break;
		case 0xAE:
			c = kvmc_compare;
			r = kvmr_accumulator;
			break;
		default:break;
		}
		
	}
	else {
		// Perform the more specific checking here.
		uint8_t bit_4 = extract_bits(instruction, 0b00010000, 4);
		uint8_t low_nibble = extract_bits(instruction, 0b00001111, 0);

		if (!bit_4) {

			// Set the proper register
			switch (low_nibble) {
			case 0x0: // Arithmetic and Logical operations
			case 0x1:
			case 0x2:
			case 0x3:
			case 0x4:
			case 0x5:
			case 0x6:
				r = kvmr_accumulator;
				break;
			case 0x7: // compare x
				r = kvmr_x_index;
				break;
			case 0xC: // Shifts and rotates
			case 0xD:
			case 0xE:
			case 0xF:
			{
				if (mode == kvma_implicit || mode == kvma_immediate) {
					r = kvmr_accumulator;
				}
				else {
					r = kvmr_none;
				}
			}
				break;
			case 0x8: // Increments
			{
				if (mode == kvma_implicit || mode == kvma_immediate) {
					r = kvmr_x_index;
				}
				else {
					r = kvmr_none;
				}
				break;
			}
			case 0x9:
				r = kvmr_y_index;
				break;
			case 0xA: // Decrements
			{
				kvm_addressing_mode mode = out_instr->addressing_mode;
				if (mode == kvma_implicit || mode == kvma_immediate) {
					r = kvmr_x_index;
				}
				else {
					r = kvmr_none;
				}
				break;
			}
			case 0xB:
				r = kvmr_y_index;
				break;
			default:break;
			}

			// Set the instruction class
			switch (low_nibble) {
			case 0x0:
				c = kvmc_and;
				break;
			case 0x1:
				c = kvmc_or;
				break;
			case 0x2:
				c = kvmc_xor;
				break;
			case 0x3:
				c = kvmc_bit_test;
				break;
			case 0x4:
				c = kvmc_add;
				break;
			case 0x5:
				c = kvmc_subtract;
				break;
			case 0x6:
			case 0x7:
				c = kvmc_compare;
				break;
			case 0x8:
			case 0x9:
				c = kvmc_increment;
				break;
			case 0xA:
			case 0xB:
				c = kvmc_decrement;
				break;
			case 0xC:
				c = kvmc_shift_left;
				break;
			case 0xD:
				c = kvmc_shift_right;
				break;
			case 0xE:
				c = kvmc_rotate_left;
				break;
			case 0xF:
				c = kvmc_rotate_right;
				break;
			default:
				break;
			}
		}
		else {
			// Load, Store, and Branch operations (and CPY)

			// Set the proper register/flag.
			switch (low_nibble)
			{
			// loads and stores
			case 0x0:
			case 0x4:
				r = kvmr_accumulator;
				break;
			case 0x1:
			case 0x5:
				r = kvmr_x_index;
				break;
			case 0x2:
			case 0x3:
			case 0x6:
				r = kvmr_y_index;
				break;
			// branches
			case 0x8:
			case 0x9:
				r = kvmr_flag_carry;
				break;
			case 0xA:
			case 0xB:
				r = kvmr_flag_zero;
				break;
			case 0xC:
			case 0xD:
				r = kvmr_flag_negative;
				break;
			case 0xE:
			case 0xF:
				r = kvmr_flag_overflow;
				break;
			default:
				break;
			}

			// Set the instruction class.
			switch (low_nibble)
			{
				// loads and stores
			case 0x0:
			case 0x1:
			case 0x2:
				c = kvmc_load;
				break;
			case 0x6:
			case 0x4:
			case 0x5:
				c = kvmc_store;
				break;
			case 0x3:
				c = kvmc_compare;
				break;
				// branches
			case 0x8:
			case 0xA:
			case 0xC:
			case 0xE:
				c = kvmc_branch_if_clear;
				break;
			case 0x9:
			case 0xB:
			case 0xD:
			case 0xF:
				c = kvmc_branch_if_set;
				break;
			default:
				break;
			}
		}
	}

	instr_set(out_instr, 0, 0, c, r);
	out_instr->is_finished_constructing = true;
}

static void print_instr(kvm_instruction* instr) {
	printf("Size: %d\nAddr. Mode: %d\nInstr. Class: %d\nReg. Operand: %d\n", instr->instruction_size, instr->addressing_mode, instr->instruction_class, instr->register_operand);
}

#pragma endregion

void kvm_cpu_decode_instr(kvm_cpu* cpu, uint8_t instruction) {
	
	instr_reset_defaults(cpu->current_instruction);

	decode_instr_size(instruction, cpu->current_instruction);
	decode_instr_addressing_mode(instruction, cpu->current_instruction);
	decode_instr_class(instruction, cpu->current_instruction);

	//print_instr(cpu->current_instruction);
}

void kvm_cpu_execute_instr(kvm_cpu* cpu, kvm_memory* mem) {
	kvm_instruction* instr = cpu->current_instruction;
}


void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem) {
	uint16_t pc = cpu->program_counter;

	uint8_t current_opcode = kvm_cpu_fetch_byte(mem, pc++);

	kvm_cpu_decode_instr(current_opcode, cpu->current_instruction);



	// Operand fetch
	switch (cpu->current_instruction->instruction_size) {
	case kvms_med:		// Fetch one more byte
		cpu->current_instruction->byte1 = kvm_cpu_fetch_byte(mem, pc++);
		break;
	case kvms_large:	// Fetch two more bytes
		cpu->current_instruction->byte1 = kvm_cpu_fetch_byte(mem, pc++);
		cpu->current_instruction->byte2 = kvm_cpu_fetch_byte(mem, pc++);
		break;
	default:break;
	}

	kvm_cpu_execute_instr(cpu, mem);
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

	cpu->current_instruction = instruction;
	/*
	Idea is to check the opcode.
	If it's not in odd opcodes out, you can easily set the type. Otherwise manually set the type.
	*/
	for (int i = 0; i < 256; i++) {
		int value = 0;
		if (i < 0x11) {
			// First few implicit opcodes.
			value = 1;
		}
		else if (i <= 0b10111111 && i >= 0b10111000) {
			// Out of place arithmetic opcodes
			value = 1;
		}
		else {
			switch (i) {
			case 0x89: // JMP Indirect
			case 0x93: // LDA Absolute, Y
			case 0xB7: // STA Absolute, Y
			case 0xA3: // LDA Indexed Indirect, X
			case 0xA7: // STA Indexed Indirect, X
			case 0xAB: // LDA Indirect Indexed, Y
			case 0xAF: // STA Indirect Indexed, Y
			case 0xE9: // JMP Absolute
			case 0xEB: // JSR Absolute

			case 0xA8: // Indirect Indexed, Y operations (They don't fit well)
			case 0xA9:
			case 0xAA:
			case 0xAC:
			case 0xAD:
			case 0xAE:
				value = 1;
				break;
			default:
				break;
			}
		}

		odd_opcodes_out[i] = value;
	}

	return cpu;
}

void kvm_cpu_free(kvm_cpu* cpu) {
	if (cpu) {
		if (cpu->current_instruction) free(cpu->current_instruction);
		free(cpu);
	}
}