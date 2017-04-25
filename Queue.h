
/* 
 * File:   Queue.h
 * Author: Yogesh Kumar
 * Purpose : This file contains the prototypes and describes how to use queue
 *          module to manage the web request queue.
 * Created on June 5, 2016, 11:25 AM
 */

#define QUEUE_H

#include<stdio.h>
#include<stdbool.h>
#include<pthread.h>

/**
 * Request Control Block.
 */
typedef struct rcb {
    int seq_no; //Sequence number of the request.
    int network; //Network descriptor.
    FILE *fp; //Pointer to the file requested.
    int fd; //fd for file.
    long sent; //No of bytes already sent.
    long sizeOfFile; //Size of the file.
    char *requestLines[3]; //request in a string. Eg : GET /file.html HTTP/1.1
    bool isInSession; //Checks if the request is current being served. Mainly useful for round robin.
    int level; //For multilevel queue with feedback. 1- 8KB, 2- 64 KB, 3- round robin
} * RCB;

/*
 * Node of the queue.
 */
typedef struct node {
    RCB info; //Data of each node.
    struct node *ptr; //pointer to the next node.
} * Node;

/*
 * Queue for the requests.
 */
typedef struct queue {
    Node front; //front node.
    Node rear; //rear node.
    int size; //size of the queue.
} * Queue;

typedef struct cond_node {
    pthread_cond_t* data;
    struct cond_node* next;
} * CondNode;

typedef struct cond_queue {
    CondNode front;
} * ConditionQueue;


/**
 * Create the queue.
 */
extern void create(Queue queue);

/**
 * Returns the size of the queue.
 * @return 
 */
extern int getSize(Queue queue);

/**
 * For inserting an item to the queue in sorted order of file size.
 * @param data
 */
extern void enque_SJF(Queue queue, RCB data);

/**
 * For inserting an item to the queue.
 * @param data
 */
extern void enque(Queue, RCB data);

/**
 * Getting an item out of the queue.
 * @return 
 */
extern RCB deque(Queue queue);

/**
 * Returns the first element of the queue.
 * @return 
 */
extern RCB frontelement(Queue queue);

/**
 * Enqueues a thread waiting condition to the suspended threads queue.
 * @param cond_worker is the condition on which the thread is waiting.
 * @param q is the suspended queue.
 */
extern void enque_Cond(pthread_cond_t* cond_worker, ConditionQueue q);

/**
 * Returns the first element from the suspended queue.
 * @param queue is the suspended queue.
 * @return 
 */
extern pthread_cond_t* DequeCond(ConditionQueue queue);

/**
 * For initializing the suspended queue.
 * @param q is the suspended queue.
 */
extern void create_condQ(ConditionQueue q) ;


