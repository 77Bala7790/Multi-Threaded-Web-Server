/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include "Queue.h"
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

int count = 0;


//void *malloc(size_t size);

/**
 * For creating the queue.
 */
void create(Queue queue) {
    queue->front = queue->rear = NULL;
    queue->size = 0;
}

/**
 * For creating the condition variable queue.
 */
void create_condQ(ConditionQueue q) {
    q->front = NULL;
}

/* Returns queue size */
int getSize(Queue queue) {
    return queue->size;
}

/**
 * Enqueing in the order of increasing file size.
 * @param data
 */
void enque_SJF(Queue que, RCB data) {
    Node temp = que->front;
    while (1) {
        if (que->front == NULL) { //if the queue is empty.
            Node newnode = (struct node *) malloc(1 * sizeof (struct node));
            newnode->info = data;
            newnode->ptr = NULL;
            que->front = newnode;
            que->rear = que->front;
            break;
        } else {
            if (temp->ptr == NULL) { //if there is only one element in queue.
                Node newnode = (struct node *) malloc(1 * sizeof (struct node));
                newnode->info = data;
                if (temp->info->sizeOfFile > data->sizeOfFile) { //if the size of file of current node is greater, then enqueue in front.
                    newnode->ptr = temp;
                    que->front = newnode;
                    break;
                } else { //else enqueue at the rear.
                    newnode->ptr = NULL;
                    temp->ptr = newnode;
                    break;
                }
            } else {
                if (temp->ptr->info->sizeOfFile > data->sizeOfFile) { //if size of file of current node is greater than data node, then insert the node. 
                    Node newnode = (struct node *) malloc(1 * sizeof (struct node));
                    newnode->info = data;
                    newnode->ptr = temp->ptr;
                    temp->ptr = newnode;
                    if (temp == que->front)
                        que->front = newnode;
                    break;
                } else
                    temp = temp->ptr;
            }
        }
    }
    que->size++;
}

/**
 * This method enqueues the web request to the queue.
 * @param que is the queue in which web request is to be enqueued.
 * @param data is the web request to be enqueued.
 */
void enque(Queue que, RCB data) {
    Node temp = que->front;

    if (que->front == NULL) { //if the queue is empty.
        Node newnode = (Node) malloc(1 * sizeof (struct node)); //assign the memory
        newnode->ptr = NULL; //Assign NULL to next pointer.
        newnode->info = data; //Assign the data to front node.
        que->front = newnode;
        que->rear = que->front; //Make front and rear equal.        
        que->size++;
        return;
    } else {
        while (1) {
            if (temp->ptr == NULL) { //if next node of queue in empty, insert the web request to the empty node.
                Node newnode = (struct node *) malloc(1 * sizeof (struct node)); // Initialize a new node and assign memory to it.
                newnode->info = data; // Assign web request data to it.
                newnode->ptr = NULL;
                temp->ptr = newnode; // Add the node to queue.
                que->size++;
                break; // Update found to true, so that while loop ends.
            } else
                temp = temp->ptr; //If the next node is not empty, move to the next node.
        }
    }

}

/**
 * This method dequeues the element from the front of the queue.
 * @param queue
 * @return 
 */
RCB deque(Queue queue) {
    if (queue != NULL) { //Check if the queue is empty.
        Node front1 = queue->front; //assign the from of the queue to a temporary variable.
        RCB data = NULL;

        if (front1 == NULL) {
            return data;
        } else {
            if (front1->ptr != NULL) {
                front1 = front1->ptr;
                data = queue->front->info;
                free(queue->front);
                queue->front = front1;
            } else {
                data = queue->front->info;
                free(queue->front);
                queue->front = NULL;
                queue->rear = NULL;
            }
            if (queue->size != 0)
                queue->size--;
        }
        return data;
    } else
        return NULL;
}

/* Returns the front element of queue */
RCB frontelement(Queue queue) {
    if ((queue->front != NULL) && (queue->rear != NULL))
        return (queue->front->info);
    else
        return NULL;
}

/**
 * This method enqueues the condition variables to the queue.
 */
void enque_Cond(pthread_cond_t* cond_worker, ConditionQueue q) {
    CondNode temp = q->front;
    if (temp == NULL) {
        CondNode newnode = (struct cond_node*) malloc(1 * sizeof (struct cond_node));
        newnode->data = cond_worker;
        newnode->next = NULL;
        q->front = newnode;
        return;
    } else {
        while (1) {
            if (temp->next == NULL) {
                CondNode newnode = (struct cond_node*) malloc(1 * sizeof (struct cond_node));
                newnode->data = cond_worker;
                newnode->next = NULL;
                temp->next = newnode;
                break;
            } else
                temp = temp->next;
        }
    }
}

pthread_cond_t* DequeCond(ConditionQueue queue) {
    if (queue != NULL) { //Check if the queue is empty.
        CondNode front1 = queue->front; //assign the from of the queue to a temporary variable.
        pthread_cond_t* data = NULL;

        if (front1 == NULL) {
            return data;
        } else {
            if (front1->next != NULL) {
                front1 = front1->next;
                data = queue->front->data;
                free(queue->front);
                queue->front = front1;
            } else {
                data = queue->front->data;
                free(queue->front);
                queue->front = NULL;
            }
        }
        return data;
    } else
        return NULL;
}
