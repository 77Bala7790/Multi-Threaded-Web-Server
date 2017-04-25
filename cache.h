/* 
 * File:   Queue.h
 * Author: Yogesh Kumar
 * Purpose : This file contains the prototypes and describes how to use queue
 *          module to manage the cache api.
 * Created on July 20, 2016, 11:25 AM
 */

#define CACHE_H
#include<stdbool.h>
#include<sys/stat.h>
#include<pthread.h>


typedef struct file_control_block{
    //int descriptor; //cfd of this node.
    ino_t inode; //inode of the file.
    char *fp;   //actual file data.
    int size;  //size of the file.
    bool isinuse; //flag to check if the node is in use.
    struct file_control_block *next;  //pointer to next node in the cache.
    pthread_mutex_t mutex;  //lock associated with every node.
}* FCB;

typedef struct cache_linked_list{
    FCB head;   //head of the list.
    FCB tail;   //tail of the list
    int size;  //size of the list.
    int leftbytes; // Number of bytes left in the list.
}* CacheList;

typedef struct file_descriptor_table_entry{
    int fd;//cache file descriptor.
    FCB fcb;// file control block.
    int offset; //offset of the file.
}* FDTE;

extern CacheList cachelist;
/**
 * Method to initialize the cache.
 * @param size is the size of the cache.
 */
extern void cache_init( int size );

/**
 * Method to read the file into/the cache.
 * @param file is the name of the file.
 * @return the file descriptor.
 */
extern int cache_open( char *file );

/**
 * Returns the size of the file.
 * @param cfd is the descriptor of the file.
 * @return
 */
extern int cache_filesize( int cfd );

/**
 * 
 * @param cfd
 * @param client
 * @param n
 * @return 
 */
extern int cache_send( int cfd, int client, int n );

/**
 * Closes the cached file descriptor.
 * @param cfd is the descriptor of the file.
 * @return 
 */
extern int cache_close( int cfd );



