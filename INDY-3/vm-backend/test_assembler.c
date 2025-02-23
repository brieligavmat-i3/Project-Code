/*	File which exists to test ksu micro's assembler.
*	Author: Matthew Watson
*/

#include <stdio.h>

#include "leakcheck_util.h"
#include "assembler.h"


void quit(void);

int main(int argc, char* argv[]) {
	char filename[50];
	
	printf("Filename to assemble: ");
	if (scanf("%50s", filename) != 1 ) return -1;

	assemble_file(filename, "out.bin");
	
	quit();

	return 0;
}

void quit(void) {
	printf("\n");
	print_allocation_data();
	clean_allocation();
}