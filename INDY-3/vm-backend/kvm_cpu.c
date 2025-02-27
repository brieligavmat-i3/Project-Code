/*	Implementation of the KSU Micro VM CPU
*	Author: Matthew Watson
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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

	out_instr->lowbyte = 0;
	out_instr->highbyte = 0;
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

void kvm_cpu_decode_instr(kvm_instruction *out_instr, uint8_t instruction) {
	
	instr_reset_defaults(out_instr);

	decode_instr_size(instruction, out_instr);
	decode_instr_addressing_mode(instruction, out_instr);
	decode_instr_class(instruction, out_instr);

	//print_instr(cpu->current_instruction);
}

#pragma region Helper functions for execution.
static void cpu_set_processor_status(kvm_cpu* cpu, bool carry, bool zero, bool interrupt_disable, bool break_command, bool twos_complement_overflow, bool negative) {
	uint8_t carry_set = (uint8_t)carry;
	uint8_t zero_set = (uint8_t)zero << 1;
	uint8_t interrupt_disable_set = (uint8_t)interrupt_disable << 2;

	uint8_t break_command_set = (uint8_t)break_command << 4;
	uint8_t twos_complement_overflow_set = (uint8_t)twos_complement_overflow << 6;
	uint8_t negative_set = (uint8_t)negative << 7;

	cpu->processor_status = carry_set | zero_set | interrupt_disable_set | break_command_set | twos_complement_overflow_set | negative_set;
}

static void cpu_set_status_flag(kvm_cpu* cpu, uint8_t flag, bool set) {
	if (set) {
		// set a flag
		cpu->processor_status |= flag;
	}
	else {
		// clear one flag
		cpu->processor_status &= (0xFF ^ flag);
	}
}

// Most operations set the zero and negative flags exclusively. This does that.
static void update_zero_and_negative_flags(kvm_cpu* cpu, uint8_t value) {
	// Set status flags
	bool zero_set = (value == 0);
	bool neg_set = extract_bits(value, 0x80, 0); // check bit 7

	cpu_set_status_flag(cpu, CPU_ZERO_FLAG, zero_set);
	cpu_set_status_flag(cpu, CPU_NEGATIVE_FLAG, neg_set);
}

static void push_stack(kvm_cpu* cpu, kvm_memory* mem, kvm_register_operand r) {
	uint8_t lowbyte = 0, highbyte = 0;
	uint8_t stptr = cpu->stack_ptr;
	bool twobytes = false;
	switch (r) {
	case kvmr_accumulator:
		highbyte = cpu->accumulator;
		break;
	case kvmr_processor_status:
		highbyte = cpu->processor_status;
		break;
	default:
		// program counter
		lowbyte = cpu->program_counter & 0x00FF;
		highbyte = cpu->program_counter >> 8;
		twobytes = true;
		break;
	}

	mem->data[stptr + STACK_PTR_OFFSET] = highbyte;
	stptr--;

	if (twobytes) {
		mem->data[stptr + STACK_PTR_OFFSET] = lowbyte;
		stptr--;
	}
	// TODO: add bounds checking, throw stack overflow error.

	cpu->stack_ptr = stptr;
}

static uint16_t merge_instr_bytes(kvm_instruction* instr) {
	return (instr->lowbyte | (uint16_t)instr->highbyte << 8);
}

static void pull_stack(kvm_cpu* cpu, kvm_memory* mem, kvm_register_operand r) {
	uint8_t lowbyte = 0, highbyte = 0;
	uint8_t stptr = cpu->stack_ptr;
	bool twobytes = (r == kvmr_none);

	stptr++;
	lowbyte = mem->data[stptr + STACK_PTR_OFFSET];

	if (twobytes) {
		stptr++;
		highbyte = mem->data[stptr + STACK_PTR_OFFSET];
	}

	// TODO: add bounds checking, throw stack overflow error.

	switch (r) {
	case kvmr_accumulator:
		cpu->accumulator = lowbyte;

		// Pull accumulator is the only stack operation which updates status flags.
		update_zero_and_negative_flags(cpu, lowbyte);
		break;
	case kvmr_processor_status:
		cpu->processor_status = lowbyte;
		break;
	default:
		// program counter
		cpu->program_counter = lowbyte | ((uint16_t)highbyte << 8);
		break;
	}

	cpu->stack_ptr = stptr;
}

static void get_address_and_value(kvm_cpu* cpu, kvm_memory* mem, kvm_instruction* instr, uint8_t* out_value, uint16_t* out_address) {
	uint16_t target_address = (uint16_t)instr->lowbyte;
	uint8_t value_at_address = 0;

	// All large instructions require the full two-byte address.
	if (instr->instruction_size == kvms_large) {
		target_address = merge_instr_bytes(instr);
	}

	switch (instr->addressing_mode)
	{
	case kvma_zpx:
	case kvma_abx:
		target_address += cpu->x_index;
		break;
	case kvma_aby:
	case kvma_zpy:
		target_address += cpu->y_index;
		break;
	case kvma_indx:
	{
		target_address += cpu->x_index;
		uint8_t lbyte, hbyte;

		lbyte = mem->data[target_address];
		hbyte = mem->data[target_address + 1];

		target_address = lbyte | ((uint16_t)hbyte << 8);
	}
	case kvma_yind:
	{
		uint8_t lbyte, hbyte;

		lbyte = mem->data[target_address];
		hbyte = mem->data[target_address + 1];

		target_address = lbyte | ((uint16_t)hbyte << 8);
		target_address += cpu->y_index;
	}
	default:
		break;
	}

	if (instr->addressing_mode == kvma_immediate) {
		// Immediately load the value and then return.
		value_at_address = (uint8_t)target_address;
	}
	else {
		value_at_address = mem->data[target_address];
	}

	*out_address = target_address;
	*out_value = value_at_address;
}



#pragma endregion

void kvm_cpu_execute_instr(kvm_cpu* cpu, kvm_memory* mem) {
	kvm_instruction* instr = cpu->current_instruction;

	uint16_t merged_instr_bytes = 0;
	if (instr->instruction_size == kvms_large) {
		merged_instr_bytes = merge_instr_bytes(instr);
	}

	// For regular operations
	uint16_t target_address;
	uint8_t value_at_address;

	get_address_and_value(cpu, mem, instr, &value_at_address, &target_address);

	switch (instr->instruction_class) {
	case kvmc_return:
		// Return from subroutine or interrupt
		pull_stack(cpu, mem, kvmr_none);
		break;
	case kvmc_transfer: // TXA and TYA
		if (instr->register_operand == kvmr_x_index) {
			// TXA
			cpu->accumulator = cpu->x_index;
		}
		else {
			// TYA
			cpu->accumulator = cpu->y_index;
		}
		update_zero_and_negative_flags(cpu, cpu->accumulator);
		break;
	case kvmc_transfer_accumulator:
		update_zero_and_negative_flags(cpu, cpu->accumulator);
		if (instr->register_operand == kvmr_x_index) {
			// TAX
			cpu->x_index = cpu->accumulator;
		}
		else {
			// TAY
			cpu->y_index = cpu->accumulator;
		}
		break;
	case kvmc_transfer_stack:
		if (instr->register_operand == kvmr_x_index) {
			// TXS
			cpu->stack_ptr = cpu->x_index;
		}
		else {
			// TSX
			cpu->x_index = cpu->stack_ptr;
			update_zero_and_negative_flags(cpu, cpu->x_index);
		}
		break;
	case kvmc_stack_push:
		push_stack(cpu, mem, instr->register_operand);
		break;
	case kvmc_stack_pull:
		pull_stack(cpu, mem, instr->register_operand);
		break;
	case kvmc_set_flag:
		switch (instr->register_operand) {
		case kvmr_flag_carry:
			cpu->processor_status |= CPU_CARRY_FLAG;
			uint8_t carry = cpu->processor_status & CPU_CARRY_FLAG;

			break;
		}
		break;
	case kvmc_clear_flag:
		switch (instr->register_operand) {
		case kvmr_flag_carry:
			cpu->processor_status = cpu->processor_status & (0xFF ^ CPU_CARRY_FLAG);
			break;
		case kvmr_flag_overflow:
			cpu->processor_status = cpu->processor_status & (0xFF ^ CPU_OVERFLOW_FLAG);
			break;
		}
		break;
	case kvmc_and:
		cpu->accumulator &= value_at_address;
		update_zero_and_negative_flags(cpu, cpu->accumulator);
		break;
	case kvmc_or:
		cpu->accumulator |= value_at_address;
		update_zero_and_negative_flags(cpu, cpu->accumulator);
		break;
	case kvmc_xor:
		cpu->accumulator ^= value_at_address;
		update_zero_and_negative_flags(cpu, cpu->accumulator);
		break;
	case kvmc_bit_test:
	{
		uint8_t test_value = cpu->accumulator & value_at_address;
		cpu_set_status_flag(cpu, CPU_ZERO_FLAG, (test_value == 0));
		cpu_set_status_flag(cpu, CPU_NEGATIVE_FLAG, (value_at_address & 0x80));
		cpu_set_status_flag(cpu, CPU_OVERFLOW_FLAG, (value_at_address & 0x40)); // set bit 6.
	}
		break;
	case kvmc_add:
	{
		uint8_t carry = cpu->processor_status & CPU_CARRY_FLAG;

		uint8_t op1 = cpu->accumulator, op2 = value_at_address;
		uint8_t result = op1 + op2 + carry;
	
		// Unsigned addition overflow (set or clear carry flag)
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, result < op1);

		/*
		* to test overflow flag (twos-complement overflow):
		* XOR bit 7 of both operands. if 1, clear overflow flag
		* AND bit 7 of both operands
		* Compare it with bit 7 of the result.
		* Should be the same, if not, set overflow flag.
		*/

		// If they're not the same sign, clear the flag.
		// Otherwise, check the sign bits of the operands and the result. If they are not the same, set the flag.
		bool same_sign = !((op1 ^ op2) >> 7);
		cpu_set_status_flag(cpu, CPU_OVERFLOW_FLAG, same_sign && op1 >> 7 != result >> 7);
		
		update_zero_and_negative_flags(cpu, result);

		cpu->accumulator = result;
	}
		break;
	case kvmc_subtract:
	{
		uint8_t carry = cpu->processor_status & CPU_CARRY_FLAG;

		uint8_t op1 = cpu->accumulator, op2 = value_at_address;
		uint8_t result = op1 - op2 - (1 - carry);

		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, result < op1);

		/*
		* to test overflow flag (twos-complement overflow):
		* XOR bit 7 of both operands. if 0, clear overflow flag (if the signs are the same, it can't overflow).
		* AND bit 7 of op1 and !bit7 of op2
		* Compare with bit 7 of result.
		* Should be the same, if not, set overflow flag.
		*/

		bool same_sign = !((op1 ^ op2) >> 7);
		//cpu_set_status_flag(cpu, CPU_OVERFLOW_FLAG, !same_sign && (op1& (op2 ^ 0xFF)) >> 7 != (result >> 7));
		cpu_set_status_flag(cpu, CPU_OVERFLOW_FLAG, !same_sign && op1 >> 7 != result >> 7);

		update_zero_and_negative_flags(cpu, result);

		cpu->accumulator = result;
	}
		break;
	case kvmc_compare:
	{
		/* CMP, CPX, and CPY compare a register value with a memory value.
		* It does this by subtracting register - memory value
		* If the register >= the memory value, carry flag is set
		* If the result is 0, the zero flag is set (the operands are equal)
		* If bit 7 is set, set the negative flag.
		* 
		* The only saved result of this instruction is in the status flags.
		*/

		uint8_t current = 0;
		switch (instr->register_operand) {
		case kvmr_accumulator:
			current = cpu->accumulator;
			break;
		case kvmr_x_index:
			current = cpu->x_index;
			break;
		case kvmr_y_index:
			current = cpu->y_index;
			break;
		default:
			break;
		}

		uint8_t result = current - value_at_address;
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, (current >= value_at_address));
		cpu_set_status_flag(cpu, CPU_ZERO_FLAG, (result == 0));
		cpu_set_status_flag(cpu, CPU_NEGATIVE_FLAG, (result >> 7));
	}
		break;
	case kvmc_increment:
	{
		uint8_t current = 0;
		switch (instr->register_operand) {
		case kvmr_none:
			current = value_at_address;
			break;
		case kvmr_x_index:
			current = cpu->x_index;
			break;
		case kvmr_y_index:
			current = cpu->y_index;
			break;
		default:
			break;
		}

		if (instr->addressing_mode == kvma_immediate) {
			current += value_at_address;
		}
		else {
			current++;
		}
		update_zero_and_negative_flags(cpu, current);

		switch (instr->register_operand) {
		case kvmr_none:
			mem->data[target_address] = current;
			break;
		case kvmr_x_index:
			cpu->x_index = current;
			break;
		case kvmr_y_index:
			cpu->y_index = current;
			break;
		default:
			break;
		}
		
	}
		break;
	case kvmc_decrement:
	{
		uint8_t current = 0;
		switch (instr->register_operand) {
		case kvmr_none:
			current = value_at_address;
			break;
		case kvmr_x_index:
			current = cpu->x_index;
			break;
		case kvmr_y_index:
			current = cpu->y_index;
			break;
		default:
			break;
		}

		if (instr->addressing_mode == kvma_immediate) {
			current -= value_at_address;
		}
		else {
			current--;
		}
		kvm_cpu* test_cpu = cpu;
		update_zero_and_negative_flags(cpu, current);

		switch (instr->register_operand) {
		case kvmr_none:
			mem->data[target_address] = current;
			break;
		case kvmr_x_index:
			cpu->x_index = current;
			break;
		case kvmr_y_index:
			cpu->y_index = current;
			break;
		default:
			break;
		}
	}
		break;
	case kvmc_shift_left:
	{
		uint8_t old_bit_7, result;
		if (instr->addressing_mode == kvma_implicit) {
			old_bit_7 = cpu->accumulator & 0x80;
			result = cpu->accumulator << 1;
			cpu->accumulator = result;
		}
		else {
			old_bit_7 = value_at_address & 0x80;
			result = value_at_address << 1;
			mem->data[target_address] = result;
		}
		update_zero_and_negative_flags(cpu, result);
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, old_bit_7);

	}
		break;
	case kvmc_shift_right:
	{
		uint8_t old_bit_0, result;
		if (instr->addressing_mode == kvma_implicit) {
			old_bit_0 = cpu->accumulator & 0x01;
			result = cpu->accumulator >> 1;
			cpu->accumulator = result;
		}
		else {
			old_bit_0 = value_at_address & 0x01;
			result = value_at_address >> 1;
			mem->data[target_address] = result;
		}
		update_zero_and_negative_flags(cpu, result);
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, old_bit_0);

	}
		break;
	case kvmc_rotate_left:
	{
		uint8_t old_carry = cpu->processor_status & CPU_CARRY_FLAG;
		uint8_t old_bit_7, result;
		if (instr->addressing_mode == kvma_implicit) {
			old_bit_7 = cpu->accumulator & 0x80;
			result = cpu->accumulator << 1 | old_carry;
			cpu->accumulator = result;
		}
		else {
			old_bit_7 = value_at_address & 0x80;
			result = value_at_address << 1 | old_carry;
			mem->data[target_address] = result;
		}
		update_zero_and_negative_flags(cpu, result);
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, old_bit_7);

	}
		break;
	case kvmc_rotate_right:
	{
		uint8_t old_carry = (cpu->processor_status & CPU_CARRY_FLAG) << 7;
		uint8_t old_bit_0, result;
		if (instr->addressing_mode == kvma_implicit) {
			old_bit_0 = cpu->accumulator & 0x01;
			result = cpu->accumulator >> 1 | old_carry;
			cpu->accumulator = result;
		}
		else {
			old_bit_0 = value_at_address & 0x01;
			result = value_at_address >> 1 | old_carry;
			mem->data[target_address] = result;
		}
		update_zero_and_negative_flags(cpu, result);
		cpu_set_status_flag(cpu, CPU_CARRY_FLAG, old_bit_0);

	}
		break;
	case kvmc_load:
	{
		switch (instr->register_operand)
		{
		case kvmr_accumulator:
			cpu->accumulator = value_at_address;
			break;
		case kvmr_x_index:
			cpu->x_index = value_at_address;
			break;
		case kvmr_y_index:
			cpu->y_index = value_at_address;
		default:
			break;
		}

		// Set status flags
		update_zero_and_negative_flags(cpu, value_at_address);
	}
		break;
	case kvmc_store:
	{

		switch (instr->register_operand)
		{
		case kvmr_accumulator:
			value_at_address = cpu->accumulator;
			break;
		case kvmr_x_index:
			value_at_address = cpu->x_index;
			break;
		case kvmr_y_index:
			value_at_address = cpu->y_index;
		default:
			break;
		}

		mem->data[target_address] = value_at_address;
	}
		break;
	case kvmc_branch_if_clear:
	{
		bool condition = false;
		switch (instr->register_operand) {
		case kvmr_flag_carry:
			condition = !(cpu->processor_status & CPU_CARRY_FLAG);
			break;
		case kvmr_flag_zero:
			condition = !(cpu->processor_status & CPU_ZERO_FLAG);
			break;
		case kvmr_flag_negative:
			condition = !(cpu->processor_status & CPU_NEGATIVE_FLAG);
			break;
		case kvmr_flag_overflow:
			condition = !(cpu->processor_status & CPU_OVERFLOW_FLAG);
			break;
		default:
			break;
		}

		if (!condition) {
			break;
		}

		if (instr->addressing_mode == kvma_relative) {
			// Relative branching
			cpu->program_counter += (int8_t)instr->lowbyte; // Cast to signed byte to allow reverse branching.
		}
		else {
			// Absolute branching
			cpu->program_counter = merged_instr_bytes;
		}
	}
	break;
	case kvmc_branch_if_set:
	{
		bool condition = false;
		switch (instr->register_operand) {
		case kvmr_flag_carry:
			condition = cpu->processor_status & CPU_CARRY_FLAG;
			break;
		case kvmr_flag_zero:
			condition = cpu->processor_status & CPU_ZERO_FLAG;
			break;
		case kvmr_flag_negative:
			condition = cpu->processor_status & CPU_NEGATIVE_FLAG;
			break;
		case kvmr_flag_overflow:
			condition = cpu->processor_status & CPU_OVERFLOW_FLAG;
			break;
		default:
			break;
		}

		if (!condition) {
			break;
		}

		if (instr->addressing_mode == kvma_relative) {
			// Relative branching
			cpu->program_counter += (int8_t)instr->lowbyte; // Cast to signed byte to allow reverse branching.
		}
		else {
			// Absolute branching
			cpu->program_counter = merged_instr_bytes;
		}
	}
		break;
	case kvmc_jump:
		if (instr->addressing_mode == kvma_indirect) {
			// Do this crazy thing.
			
			uint8_t lbyte, hbyte;
			uint16_t actual_target = merged_instr_bytes;

			lbyte = mem->data[actual_target];
			hbyte = mem->data[actual_target + 1];

			actual_target = lbyte | ((uint16_t)hbyte << 8);
			cpu->program_counter = actual_target;

		}
		else {
			// Absolute jump (normal)
			cpu->program_counter = merged_instr_bytes; // Jump!
		}
		break;
	case kvmc_jump_to_subroutine:
		push_stack(cpu, mem, kvmr_none); // Store the current program counter.
		cpu->program_counter = merged_instr_bytes; // Jump!
		break;

	}
}


void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem) {
	uint16_t pc = cpu->program_counter;

	uint8_t current_opcode = kvm_cpu_fetch_byte(mem, pc++);

	kvm_cpu_decode_instr(cpu->current_instruction, current_opcode);


	// Operand fetch
	switch (cpu->current_instruction->instruction_size) {
	case kvms_med:		// Fetch one more byte
		cpu->current_instruction->lowbyte = kvm_cpu_fetch_byte(mem, pc++);
		break;
	case kvms_large:	// Fetch two more bytes
		cpu->current_instruction->lowbyte = kvm_cpu_fetch_byte(mem, pc++);
		cpu->current_instruction->highbyte = kvm_cpu_fetch_byte(mem, pc++);
		break;
	default:break;
	}

	cpu->program_counter = pc;

	kvm_cpu test_cpu = *cpu;

	kvm_cpu_execute_instr(cpu, mem);
	/*
	Fetch byte at program counter for opcode,
	Decode opcode 
	Based on instruction type, fetch the next 0, 1, or 2 bytes, and increment the program counter that much.
	Execute the proper instruction.
	*/
}

void kvm_cpu_print_status(kvm_cpu* cpu) {
	printf("Program Counter: %x\nAccumulator: %x\nX: %x\nY: %x\nStack Pointer: %x\nProcessor Status: %x\n", cpu->program_counter, cpu->accumulator, cpu->x_index, cpu->y_index, cpu->stack_ptr, cpu->processor_status);
}

kvm_cpu* kvm_cpu_init(void) {
	kvm_cpu* cpu = malloc(sizeof(kvm_cpu));

	cpu->accumulator = 0;
	cpu->x_index = 0;
	cpu->y_index = 0;

	cpu->stack_ptr = STACK_PTR_DEFAULT;

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