/*	File which exists to test ksu micro's assembler.
*	Author: Matthew Watson
*/

#include <stdio.h>

#include "leakcheck_util.h"


void quit(void);

int main(int argc, char* argv[]) {

	
	quit();

	return 0;
}

void quit(void) {
	printf("\n");
	print_allocation_data();
	clean_allocation();
}