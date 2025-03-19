/*	Test file for the virtual machine driver
	Author: Matthew Watson
*/
#include <stdio.h>

#include "leakcheck_util.h"
#include "kvm.h"
#include "kvm_memory.h"

void quit_message(void) {
	// Wait for the user to type something before quitting.
	printf("Press [enter] to quit.\n");
	while (getchar() != '\n');
	getchar();
}

int main(int argc, char* argv[]) {

	// Get filename from user
	printf("Files:\n");
	system("dir tests /B");

	printf("Enter filename to assemble and run (omit '.txt'): ");
	char fname[50];

	if (scanf("%s", fname) != 1) {
		printf("Error with scanf, cannot read in fname.\n");
		quit_message();
		return -1;
	}
	printf("\nInstruction Filename: %s\n", fname);

	if (kvm_init() != 0) return -1;

	if (kvm_load_instructions(fname) == -1) {
		printf("Error loading instruction file %s.\n", fname);
		quit_message();
		return -1;
	}

	// Actually run the KSU Micro VM
	int kvm_run_result = kvm_start(-1);

	if (kvm_run_result == 0) {
		printf("\nProcess Finished.\nFirst four pages of memory:\n");
		kvm_hexdump(0, 4, true);

		//printf("\n\nTile Data: \n");
		//kvm_hexdump(0x84, 4, false);
	}

	kvm_quit();

	// Finish up with the memory leak detection stuff.
	print_allocation_data();
	clean_allocation();

	quit_message();
	return 0;
}