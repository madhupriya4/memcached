////
//// Created by madhupriya on 11/21/18.
////
////

#include "memcached.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>


// A utility function to create an empty queue
struct Queue* createQueue(void)
{
    struct Queue *q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    q->curSize=0;
    q->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if(pthread_mutex_init(q->mutex, NULL)) {
        perror("LRU Cache unable to initialise mutex");
        free(q);
        return NULL;
    }

    return q;
}
// A utility function to create a new linked list node.
struct QNode* newNode(char * key,uint8_t keyLength,int valueLength)
{
    struct QNode *temp = (struct QNode*)malloc(sizeof(struct QNode));
    temp->key = (char * )malloc(keyLength* sizeof(char));
//    for (int i=0; i<keyLength; i++)
//        *(char *)(temp->key + i) = *(char *)(key + i);
    strcpy(temp->key,key);
    temp->keyLength=keyLength;
    temp->valueLength=valueLength;
    temp->next = NULL;
    return temp;
}

// The function to add a key k to q
void enQueue(struct Queue *q, char * key,uint8_t keyLength,int valueLength)
{
    // Create a new LL node
    struct QNode *temp = newNode(key,keyLength,valueLength);
    fprintf(stderr,"printing temp node's values:-\nkey=%s\nkey length=%ul\nvalue length=%d\n",key,keyLength,valueLength);


    q->curSize+=(temp->valueLength+temp->keyLength);

    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL)
    {
        q->front = q->rear = temp;
        unlock_cache();
        return;
    }

    // Add the new node at the end of queue and change rear
    q->rear->next = temp;
    q->rear = temp;



    fprintf(stderr,"\ncur size=%zu \n",q->curSize);
}

// Function to remove a key from given queue q
struct QNode *deQueue(struct Queue *q)
{
    // If queue is empty, return NULL.
    lock_cache();
    if (q->front == NULL)
    {
        unlock_cache();
        return NULL;
    }

    // Store previous front and move front one node ahead
    struct QNode *temp = q->front;
    q->curSize-=(temp->valueLength+temp->keyLength);
    q->front = q->front->next;

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL)
        q->rear = NULL;
    unlock_cache();
    return temp;


}

struct QNode *  keyNodePointer(struct Queue *q, char * key)
{

    for(struct QNode *temp=q->front;temp!=NULL;temp=temp->next)
        if (strcmp(temp->key, key) == 0)
            return temp;

    return NULL;

}

void deleteKey(struct Queue *queue, char * key)
{
    lock_cache();
    struct QNode* reqPage = keyNodePointer(queue, key);


    if ( reqPage == NULL )
    {
        unlock_cache();
        return;
    }

    else if (reqPage == queue->front && reqPage==queue->rear)
    {
        queue->front = queue->rear = NULL;
        queue->curSize=0;
        free(reqPage);
    }
    else if(reqPage == queue->front)
    {
        queue->front=queue->front->next;
        free(reqPage);
    }
    else
    {
        struct QNode* prev=queue->front;
        while(prev->next!=reqPage)
            prev=prev->next;

        if(reqPage==queue->rear)
        {
            prev->next=NULL;
            queue->rear=prev;
            free(reqPage);
        }
        else {
            prev->next = reqPage->next;
            free(reqPage);
        }

    }
    unlock_cache();
}

void ReferencePage(struct Queue *queue, char * key, uint8_t keyLength,int valueLength)
{
    lock_cache();
    struct QNode* reqPage = keyNodePointer(queue, key);

    // the page is not in cache, bring it
    if ( reqPage == NULL )
        enQueue(queue,key,keyLength,valueLength);

        // page is there and not at front, change pointer
    else if (reqPage != queue->front)
    {
        // Unlink rquested page from its current location
        // in queue.
        struct QNode* prev=queue->front;
        while(prev->next!=reqPage)
            prev=prev->next;

        prev->next = reqPage->next;
        queue->rear->next=reqPage;
        queue->rear=reqPage;
        queue->rear->next=NULL;

    }
    unlock_cache();
}

void printQueue(struct Queue *queue)
{
    fprintf(stderr,"\nCurrent lru size=%lu\n",queue->curSize);
    for(struct QNode * temp=queue->front;temp!=NULL;temp=temp->next)
        fprintf(stderr,"%s ",temp->key);
    fprintf(stderr,"\n");
}

////#include <stdio.h>
////#include <stdlib.h>
////#include "memcached.h"
////
//////static bdb_node ** bdb_malloc_buffer;
////static int bdb_lru_allowed_numbober_of_items;
////static int bdb_lru_current_number_of_items;
////bdb_node * front,*rear;
////
//////initial number of allowed items
////#define INIT_LRU_FREELIST_LENGTH 500
////
////typedef struct bdb_node
////{
////    char * key;
////    size_t item_size;
////    bdb_node * prev, * next;
////} bdb_node;
////
//////contains pointers to actual queue
////typedef struct queue_node
////
////
////void bdb_malloc_buffer_init(void) {
////
////    bdb_lru_allowed_number_of_items = INIT_LRU_FREELIST_LENGTH;
////    bdb_lru_current_number_of_items  = 0;
////
//////    bdb_malloc_buffer = (bdb_node **)malloc( sizeof(bdb_node *) * bdb_lru_allowed_number_of_items );
//////    if (bdb_malloc_buffer == NULL) {
//////        perror("malloc()");
//////    }
////    return;
////}
//
///*
// * Returns a item buffer from the freelist, if any. Should call
// * item_from_freelist for thread safty.
// * */
////bdb_node *do_item_from_freelist_bdb(void) {
////
////    bdb_node *s;
////
////    if (bdb_lru_current_number_of_items < bdb_lru_allowed_number_of_items-1) {
////        s = bdb_malloc_buffer[bdb_lru_current_number_of_items++];
////    }
////    else {
////        /*double the size of the buffer if insufficient space*/
////        item **new_bdb_malloc_buffer = (bdb_node **)realloc(bdb_malloc_buffer, sizeof(bdb_node *) * bdb_lru_allowed_number_of_items * 2);
////
////        if (new_bdb_malloc_buffer)
////        {
////            bdb_lru_allowed_number_of_items *= 2;
////            //copy items from old buffer to new buffer from front to rear and reset front and rear
////            bdb_malloc_buffer = new_bdb_malloc_buffer;
////            s=bdb_malloc_buffer[bdb_lru_current_number_of_items++];
////        }
////        else
////            s=NULL;
////    }
////
////    return s;
////}
//
//// A utility function to create a new Queue Node. The queue Node
//// will store the given key and size
////bdb_node* newbdb_node(char *key,size_t item_size)
////{
////    // Allocate memory and assign 'pageNumber'
////    bdb_node* temp = do_item_from_freelist_bdb();
////    temp->key = key;
////    temp->item_size = item_size;
////
////    // Initialize prev and next as NULL
////    temp->prev = temp->next = NULL;
////
////    return temp;
////}
//
////add to lru queue
////int add_to_bdb_lru(char * key,size_t item_size)
////{
////    rear->next=
////}