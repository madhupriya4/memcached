//
// Created by madhupriya on 11/21/18.
//

#include <stdio.h>
#include <stdlib.h>
#include "memcached.h"
//
//static bdb_node ** bdb_malloc_buffer;
//static int bdb_lru_allowed_number_of_items;
//static int bdb_lru_current_number_of_items;
//bdb_node * front,*rear;
//
////initial number of allowed items
//#define INIT_LRU_FREELIST_LENGTH 500
//
//typedef struct bdb_node
//{
//    char * key;
//    size_t item_size;
//} bdb_node;
//
////contains pointers to actual queue
//typedef struct queue_node
//
//
//void bdb_malloc_buffer_init(void) {
//
//    bdb_lru_allowed_number_of_items = INIT_LRU_FREELIST_LENGTH;
//    bdb_lru_current_number_of_items  = 0;
//
//    bdb_malloc_buffer = (bdb_node **)malloc( sizeof(bdb_node *) * bdb_lru_allowed_number_of_items );
//    if (bdb_malloc_buffer == NULL) {
//        perror("malloc()");
//    }
//    return;
//}

/*
 * Returns a item buffer from the freelist, if any. Should call
 * item_from_freelist for thread safty.
 * */
//bdb_node *do_item_from_freelist_bdb(void) {
//
//    bdb_node *s;
//
//    if (bdb_lru_current_number_of_items < bdb_lru_allowed_number_of_items-1) {
//        s = bdb_malloc_buffer[bdb_lru_current_number_of_items++];
//    }
//    else {
//        /*double the size of the buffer if insufficient space*/
//        item **new_bdb_malloc_buffer = (bdb_node **)realloc(bdb_malloc_buffer, sizeof(bdb_node *) * bdb_lru_allowed_number_of_items * 2);
//
//        if (new_bdb_malloc_buffer)
//        {
//            bdb_lru_allowed_number_of_items *= 2;
//            //copy items from old buffer to new buffer from front to rear and reset front and rear
//            bdb_malloc_buffer = new_bdb_malloc_buffer;
//            s=bdb_malloc_buffer[bdb_lru_current_number_of_items++];
//        }
//        else
//            s=NULL;
//    }
//
//    return s;
//}

// A utility function to create a new Queue Node. The queue Node
// will store the given key and size
//bdb_node* newbdb_node(char *key,size_t item_size)
//{
//    // Allocate memory and assign 'pageNumber'
//    bdb_node* temp = do_item_from_freelist_bdb();
//    temp->key = key;
//    temp->item_size = item_size;
//
//    // Initialize prev and next as NULL
//    temp->prev = temp->next = NULL;
//
//    return temp;
//}

//add to lru queue
//int add_to_bdb_lru(char * key,size_t item_size)
//{
//    rear->next=
//}