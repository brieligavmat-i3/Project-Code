#pragma once

#include <stdlib.h>

/* Linked-list node and associated functions */
typedef struct link_node link_node;
struct link_node{
    void *data;
    size_t data_size;
    link_node *next;
};

/* Initialize the linked list node */
static link_node* link_node_init(void *data, link_node *next, size_t data_size);

static void link_node_free(link_node *node);

/* Remove the node in front of the specified node */
static void link_node_remove_next(link_node *node);

// Insert a node in front of the specified node
static void link_node_insert_next(link_node *node, void *data, size_t data_size);

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* Linked-list itself and associated functions */
typedef struct{
    size_t length;
    size_t data_size;
    link_node *head;
}link_list;

link_list* link_list_init(void *head_data, size_t data_size);

// Delete all nodes in a linked list
void link_list_delete(link_list *list);

// Insert a node at the head position
void link_list_add_head(link_list *list, void *data);

// Get the element at the specified index
link_node* link_list_get_at(link_list *list, int index);

// Insert a node at the specified position. Use -1 for end of list.
void link_list_add_at(link_list *list, void *data, int index);

// Delete a node at the specified index
void link_list_remove_at(link_list *list, int index);

void print_list_addresses(link_list *list);
