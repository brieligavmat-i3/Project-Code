/*
* Author: Matthew Watson
*/

#pragma once

#include <stdint.h> // For guranteed fixed-width integers.

#include "kvm_memory.h"
/*

Hm.

fetch
decode
execute

registers:
a, x, y, stack, PC

*/

// In the memory map, 0xE000 is the start point for program ROM information.
#define PROGRAM_COUNTER_ENTRY_POINT 0xE000
#define DEFAULT_PROCESSOR_STATUS 0

typedef enum kvm_instruction_size {
	kvms_small = 1, kvms_med = 2, kvms_large = 3, 
	kvms_invalid = 0	// Used for error handling
}kvm_instruction_size;

typedef enum kvm_addressing_mode {
	kvma_invalid,		// Used for error handling

	kvma_implicit,		// These instructions only need one byte
	kvma_immediate,		// Specify a numeric litereal to operate on
	kvma_relative,		// Specify an offset to the program counter for branch instructions
	kvma_zeropage,		// Absolute addressing but with one-byte addresses
	kvma_zpx,			// Zero Page, but using x register as offset
	kvma_zpy,			// Same as above, but using y register
	kvma_absolute,		// Specify two-byte address to operate on
	kvma_indirect,		// Specify two-byte address which stores the low byte of an address to jump to
	kvma_abx,			// Absolute, but using x register as offset
	kvma_aby,			// Same as above, but using y register
	kvma_indx,			// Same as abx, except the value given is the low byte of a pointer to the target address
	kvma_yind			// Specify two-byte address storing the low byte of a target address. Add the y register to the target.
} kvm_addressing_mode;

//TODO: define instruction classes (AND, ADC, JMP, etc.)
typedef enum kvm_instruction_class {
	kvmc_invalid,
	kvmc_no_op, kvmc_force_interrupt, kvmc_return, 
	kvmc_transfer, kvmc_transfer_accumulator, 
	kvmc_stack_push, kvmc_stack_pull,
	kvmc_set_flag, kvmc_clear_flag,

	kvmc_and, kvmc_or, kvmc_xor, kvmc_bit_test,
	kvmc_add, kvmc_subtract, kvmc_compare,

	kvmc_increment, kvmc_decrement,
	kvmc_shift_left, kvmc_shift_right,
	kvmc_rotate_left, kvmc_rotate_right,

	kvmc_load, kvmc_store,

	kvmc_branch_if_clear, kvm_branch_if_set,
	kvmc_jump, kvmc_jump_to_subroutine

}kvm_instruction_class;

typedef enum kvm_register_operand {
	kvmr_invalid,

	// Registers
	kvmr_accumulator,
	kvmr_x_index,
	kvmr_y_index,
	kvmr_stack_ptr,
	kvmr_processor_status,

	// Flags (stored in processor status)
	kvmr_flag_carry,
	kvmr_flag_overflow,
	kvmr_flag_zero,
	kvmr_flag_negative,
	kvmr_flag_interrupt_disable
}kvm_register_operand;

typedef struct kvm_instruction {
	kvm_instruction_size instruction_size;
	kvm_addressing_mode addressing_mode;
	kvm_instruction_class instruction_class;
	kvm_register_operand register_operand;

	uint8_t byte1, byte2; // Byte 1 is used with instruction sizes kvmi_med and kvmi_large, while byte 2 is only used with kvmi_large.
}kvm_instruction;

typedef struct kvm_cpu {
	uint16_t program_counter;

	uint8_t accumulator;
	uint8_t x_index;
	uint8_t y_index;

	uint8_t stack_ptr;

	uint8_t processor_status;

	kvm_instruction* current_instruction;
} kvm_cpu;

uint8_t kvm_cpu_fetch_byte(kvm_memory* mem, size_t index);

void kvm_cpu_decode_instr(uint8_t instruction, kvm_instruction* out_instr);

/* Perform fetch, decode, and execute operations.
*/
void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem);

// Initializes the CPU and zeroes out its values.
kvm_cpu* kvm_cpu_init(void);

void kvm_cpu_free(kvm_cpu* cpu);

