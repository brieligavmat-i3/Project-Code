/*
* Author: Matthew Watson
*/

#pragma once

#include <stdint.h> // For guranteed fixed-width integers.
#include <stdbool.h>

#include "kvm_memory.h"

//#define PROGRAM_COUNTER_ENTRY_POINT 0x0200
#define STACK_PTR_OFFSET 0x0100
#define STACK_PTR_DEFAULT 0xFF
#define DEFAULT_PROCESSOR_STATUS 0

// Processor status flags

#define CPU_CARRY_FLAG 0x01
#define CPU_ZERO_FLAG 0x02
#define CPU_INTERRUPT_DISABLE_FLAG 0x04
#define CPU_BREAK_FLAG 0x10
#define CPU_OVERFLOW_FLAG 0x40
#define CPU_NEGATIVE_FLAG 0x80

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
	kvmc_transfer, kvmc_transfer_accumulator, kvmc_transfer_stack,
	kvmc_stack_push, kvmc_stack_pull,
	kvmc_set_flag, kvmc_clear_flag,

	kvmc_and, kvmc_or, kvmc_xor, kvmc_bit_test,
	kvmc_add, kvmc_subtract, kvmc_compare,

	kvmc_increment, kvmc_decrement,
	kvmc_shift_left, kvmc_shift_right,
	kvmc_rotate_left, kvmc_rotate_right,

	kvmc_load, kvmc_store,

	kvmc_branch_if_clear, kvmc_branch_if_set,
	kvmc_jump, kvmc_jump_to_subroutine

}kvm_instruction_class;

typedef enum kvm_register_operand {
	kvmr_invalid,

	kvmr_none,

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
	bool is_finished_constructing;

	kvm_instruction_size instruction_size;
	kvm_addressing_mode addressing_mode;
	kvm_instruction_class instruction_class;
	kvm_register_operand register_operand;

	uint8_t lowbyte, highbyte; // lowbyte is used with instruction sizes kvms_med and kvms_large, while highbyte is only used with kvms_large.
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

int odd_opcodes_out[256];

uint8_t kvm_cpu_fetch_byte(kvm_memory* mem, size_t index);

void kvm_cpu_decode_instr(kvm_instruction *out_instr, uint8_t instruction);

void kvm_cpu_execute_instr(kvm_cpu* cpu, kvm_memory* mem);

/* Perform fetch, decode, and execute operations.
*/
void kvm_cpu_cycle(kvm_cpu* cpu, kvm_memory* mem);

void kvm_cpu_print_status(kvm_cpu* cpu);

// Initializes the CPU and zeroes out its values.
kvm_cpu* kvm_cpu_init(void);

void kvm_cpu_free(kvm_cpu* cpu);

