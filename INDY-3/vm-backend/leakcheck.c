#include <stdio.h>
#include <stdlib.h>

#include "leakcheck.h"
#include "linklist.h"

static size_t current_allocated_bytes = 0; 
static link_list *allocation_record = NULL;

static allocation *create_data(void* address, size_t size);

void* leakcheck_malloc(size_t size){

    // Initialize allocation record if not done already.
    if (!allocation_record){
        // Head node is a blank allocation with 0 in both fields. This is so we don't have to delete the linked list if all memory is freed.
        allocation_record = link_list_init(create_data(0, 0), sizeof(allocation));
    }

    void *ptr = malloc(size);
    current_allocated_bytes += size;

    if (!ptr) {
        fprintf(stderr, "Fatal error, malloc() has returned a null pointer. (in leakcheck_malloc)\n");
        exit(1);
    }

    // Append the new allocation to the end of the records list.
    link_list_add_at(allocation_record, create_data(ptr, size), -1);

    //printf("Allocating %d bytes.\n", size);
    return ptr;
}

void* leakcheck_realloc(void* ptr, size_t new_size) {

    // Find the current allocation of ptr
    link_node* current = allocation_record->head->next;
    int i = 1;
    while (current != NULL) {
        // Check to see if the allocation address matches the given address.
        allocation* current_allocation = (allocation*)(current->data);
        if (!current_allocation) {
            // This should never happen.
            fprintf(stderr, "Error with leakcheck_realloc- allocation struct %d is null.\n", i);
            exit(-1);
            return 0;
        }

        if (ptr == current_allocation->ptr) {
            current_allocated_bytes -= current_allocation->size;
            link_list_remove_at(allocation_record, i);
            break;
        }

        i++;
        current = current->next;
    }

    void* new_ptr = realloc(ptr, new_size);
    if (!new_ptr) {
        fprintf(stderr, "Fatal error, realloc() has returned a null pointer. (in leakcheck_realloc)\n");
        free(ptr);
        exit(1);
    }

    current_allocated_bytes += new_size;

    // Append the new allocation to the end of the records list.
    link_list_add_at(allocation_record, create_data(new_ptr, new_size), -1);

    return new_ptr;
}

void leakcheck_free(void *ptr){
    
    // head->next because head is the blank one.
    link_node *current = allocation_record->head->next;
    int i = 1;
    while(current != NULL){
        // Check to see if the allocation address matches the given address.
        allocation *current_allocation = (allocation*)(current->data);
        if(!current_allocation){
            // This should never happen.
            fprintf(stderr, "Error with leakcheck_free- allocation struct %d is null.\n", i);
            return;
        }

        if (ptr == current_allocation->ptr){
            current_allocated_bytes -= current_allocation->size;
            link_list_remove_at(allocation_record, i);
            free(ptr);
            return;
        }
        
        i++;
        current = current->next;
    }

    fprintf(stderr, "Error with leakcheck_free- No matching address found.\n");
}

/*
// This only works if main() is not already defined by a library.
int main(int argc, char *argv[]){
    int result = leakcheck_main(argc, argv);
    clean_allocation();
    return result;
}
*/


size_t get_allocated_memory(void){
    return current_allocated_bytes;
}

// LINK LIST INTERACTION

// Creates a dynamically loaded allocation struct for the linked list.
// This is deallocated by the linked list.
static allocation *create_data(void* address, size_t size){
    allocation *result = malloc(sizeof(allocation));
    if (!result) {
        fprintf(stderr, "Fatal error: malloc failed.\n");
        exit(1);
    }

    result->ptr = address;
    result->size = size;
    //printf("Location of data at creation: %d\nAddr: %d\nSize: %d\n", result, result->ptr, result->size);
    return result;
}

void print_allocation_data(){

    if(!allocation_record){
        printf("Allocation record has not been initialized.\n");
        return;
    }

    printf("Number of allocations: %llu\nBytes allocated: %llu\n\n", allocation_record->length-1, current_allocated_bytes);

    link_node *current = allocation_record->head->next;
    int i = 1;
    while(current != NULL){
        allocation *dat = (allocation*)(current->data);
        if (!dat){
            fprintf(stderr, "Error with linked list, item %d has a null data pointer.", i);
            continue;
        }
        printf("item: %d\naddress: %llu\nsize: %d\n\n", i, (size_t)dat->ptr, (int)dat->size);
        current = current->next;
        i++;
    }
}

// Delete the linked list of allocations.
void clean_allocation(void){
    if(allocation_record) link_list_delete(allocation_record);
}