#include <stdio.h>
#include <stdlib.h>

#include "linklist.h"


/* Initialize the linked list node */
static link_node* link_node_init(void *data, link_node *next, size_t data_size){
    link_node *node = malloc(sizeof(link_node));        // allocate the memory for the node
    if (node == NULL){
        fprintf(stderr, "Fatal error! Oh no! Malloc failed!");
        exit(1);
    }

    node->data = data;
    node->next = next;
    return node;
}

static void link_node_free(link_node *node){
    if(!node) return;

    if(node->data){
        //printf("Freeing data at addr: %d\n", node->data);
        free(node->data);
    }
    free(node);
}

/* Remove the node in front of the specified node */
static void link_node_remove_next(link_node *node){
    link_node *next = node->next;

    if(next == NULL){
        printf("No next node to delete.");
        return;
    }

    // Determine the next link
    link_node *new_link = next->next;

    // Deallocate the node to be deleted
    link_node_free(next);

    node->next = new_link;
}

// Insert a node in front of the specified node
static void link_node_insert_next(link_node *node, void *data, size_t data_size){
    link_node *current_next = node->next;
    link_node *new_node = link_node_init(data, current_next, data_size);
    node->next = new_node;
}

link_list* link_list_init(void *head_data, size_t data_size){
    link_list *list = malloc(sizeof(link_list));
    if (list == NULL){
        printf("Malloc failed to create the list.");
        exit(1);
    }
    list->data_size = data_size;
    list->length = 1;                                               // Initialize the length to 1, since we're starting with one node
    link_node *head = link_node_init(head_data, NULL, data_size);   // Create the head node
    list->head = head;                                              // Put the head on the list

    return list;
}

// Delete all nodes in a linked list followed by the list itself.
void link_list_delete(link_list *list){

    // Delete all nodes but the head
    link_node *current = list->head;
    while(current->next != NULL){
        link_node_remove_next(current);
    }

    link_node_free(list->head);   // Get rid of the head node
    free(list);         // Get rid of the list itself
}

// Insert a node at the head position
void link_list_add_head(link_list *list, void *data){
    link_node *new_node = link_node_init(data, list->head, list->data_size);

    // Swap the head off
    list->head = new_node;

    // Increment the list length
    list->length++;
}

// Get the element at the specified index
link_node* link_list_get_at(link_list *list, int index){

    // Bit of bound checking
    if (index >= list->length || index == -1){
        index = (int)list->length - 1;
    }
    else if (index < 0){
        index = 0;
    }

    // Start counting at the first node
    link_node *current_node = list->head;

    for(int i = 0; i < index; i++){
        current_node = current_node->next;
    }

    return current_node;
}

// Insert a node at the specified position. Use -1 for end of list.
void link_list_add_at(link_list *list, void *data, int index){


    // Start counting at the first node
    link_node *current_node = link_list_get_at(list, index);

    // Insert the node
    link_node_insert_next(current_node, data, list->data_size);
    list->length++;
}

// Delete a node at the specified index
void link_list_remove_at(link_list *list, int index){

    // If there is only one element, don't bother deleting since you can just delete the list.
    if (list->length == 1){
        return;
    }

    // Decrement the list length
    list->length--;

    // Do this if you're removing the first element
    if (index == 0){
        link_node *current_head = list->head;
        link_node *next = current_head->next;

        link_node_free(current_head);

        list->head = next;
        current_head = NULL;
        return;
    }

    // Make sure the end of the list is in the correct form
    if (index == -1){
        index = (int)list->length;
    }

    link_node *previous_node = link_list_get_at(list, index-1);
    link_node_remove_next(previous_node);
}

/*
void print_list_addresses(link_list *list){

    printf("List length: %llu\n", (unsigned long long)list->length);

    link_node *current = list->head;
    while(current->next != NULL){
        printf("%d, ", (int)current->data);
        current = current->next;
    }
    printf("%d.", (int)current->data);
}
*/
