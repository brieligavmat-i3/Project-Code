#pragma once

/*
This header provides wrappers around malloc() and free() which use a linked-list to keep track of memory allocations and deallocations.

After you are done, call clean_allocation() to free the linked-list itself.
*/

#include <stdlib.h>

typedef struct{
    size_t size;
    void* ptr;
}allocation;

void* leakcheck_malloc(size_t size);

void leakcheck_free(void *ptr);

// Return the number of currently allocated bytes.
int get_allocated_memory(void);

void print_allocation_data(void);
void clean_allocation(void);

// Wrapper around main() which calls main() and calls clean_allocation() after.
int leakcheck_main(int argc, char *argv[]);
