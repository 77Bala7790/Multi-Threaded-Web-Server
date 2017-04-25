
#include "cache.h"
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<unistd.h>

FDTE FileDescriptorTable[100];
ino_t FileBeingLoaded[100];
ino_t SWOFT[100];

pthread_mutex_t mutex_1_fileLoaded;
pthread_mutex_t mutex_1_fileLoading;
pthread_mutex_t mutex_1_head;
pthread_mutex_t mutex_2_tail;
pthread_mutex_t mutex_3_insert;
pthread_mutex_t mutex_3_FDTable;
pthread_mutex_t mutex_1_Cache;
pthread_mutex_t mutex_1_SWOFT;

int assignDescriptor(FCB block);
void deleteSwoftEntry(ino_t inode);
void fileLoad(ino_t inode);
bool isFileLoading(ino_t inode);
void release(FCB temp);
void fileLoaded(ino_t inode);
bool isBeingServed(FCB file);
bool fileInCache(ino_t inode);
void swoftEntry(ino_t inode);
int findinCache(ino_t inode);

/**
 * Method to initialize the cache with proper size.
 * @param size is the size of cache.
 */
void cache_init(int size) {
    cachelist = (struct cache_linked_list *) malloc(1 * sizeof (struct cache_linked_list)); //assigning memory to the linked list.
    cachelist->size = size;
    cachelist->leftbytes = size;
    cachelist->head = NULL;
    cachelist->tail = cachelist->head;
    pthread_mutex_init(&mutex_1_Cache, NULL);
    pthread_mutex_init(&mutex_1_SWOFT, NULL);
    pthread_mutex_init(&mutex_1_head, NULL);
    pthread_mutex_init(&mutex_2_tail, NULL);
    pthread_mutex_init(&mutex_3_insert, NULL);
    pthread_mutex_init(&mutex_3_FDTable, NULL);
    pthread_mutex_init(&mutex_1_fileLoaded, NULL);
    pthread_mutex_init(&mutex_1_fileLoading, NULL);
}

/**
 * Method to read the file into/the cache.
 * @param file is the name of the file.
 * @return the file descriptor.
 */
int cache_open(char *file) {

    bool wasloading = false;

    //finding file size and inode.
    struct stat st;
    ino_t inode;
    stat(file, &st);
    inode = st.st_ino;
    off_t file_size;
    file_size = st.st_size;
    char *size = (char*) malloc(file_size);
    sprintf(size, "%jd", file_size);

    if (inode != 0) {

        while (isFileLoading(inode) && fileInCache(inode)) { //if it is loading keep looping.                    
        }
        int filedesc = 0;
        filedesc = findinCache(inode);
        if (filedesc != 0) { //if the file is in cache. assign the descriptor and return it.
            return filedesc;
        } else {
            pthread_mutex_lock(&mutex_1_fileLoading);

            bool desc = fileInCache(inode);
            if (desc) {
                while (isFileLoading(inode)) { //if it is loading keep looping.                    
                }
                wasloading = true;
            }
            if (wasloading) {//&& (atoi(size) < cachelist->size)) {
                pthread_mutex_unlock(&mutex_1_fileLoading);
                return findinCache(inode);
            } else {
                if (atoi(size) < cachelist->size) {
                    fileLoad(inode);
                    swoftEntry(inode);
                    pthread_mutex_unlock(&mutex_1_fileLoading);
                    bool isenoughspace = false;
                    while (!isenoughspace) { //checking if there is enough space for the file to be inserted into the cache.
                        if (atoi(size) > cachelist->leftbytes) {
                            pthread_mutex_lock(&cachelist->head->mutex);
                            FCB temp = cachelist->head;
                            if (cachelist->head == cachelist->tail) {
                                cachelist->tail = NULL;
                            }
                            FCB fileBeingServed = cachelist->head;
                            while (isBeingServed(fileBeingServed)) {
                            }
                            deleteSwoftEntry(cachelist->head->inode);
                            cachelist->head = cachelist->head->next;

                            pthread_mutex_lock(&mutex_1_Cache);
                            cachelist->leftbytes += temp->size;
                            pthread_mutex_unlock(&mutex_1_Cache);
                            printf("File of size %d evicted.\n", temp->size);
                            fflush(stdout);
                            pthread_mutex_unlock(&temp->mutex);
                            release(temp);
                        } else
                            isenoughspace = true;
                    }


                    //inserting the file to the cache.
                    FCB newnode = (struct file_control_block *) malloc(1 * sizeof (struct file_control_block)); //assigning memory to the node.
                    //newnode->descriptor = globalDescriptor;
                    //globalDescriptor++;
                    newnode->inode = inode;
                    pthread_mutex_init(&newnode->mutex, NULL);
                    newnode->size = atoi(size);
                    newnode->fp = (char*) malloc(newnode->size);
                    FILE *f = fopen(file, "rb");
                    fread(newnode->fp, newnode->size, 1, f);
                    fclose(f);
                    newnode->next = NULL;
                    pthread_mutex_lock(&newnode->mutex);
                    FCB temptail = NULL;
                    pthread_mutex_lock(&mutex_1_Cache);
                    if (cachelist->tail != NULL) {
                        pthread_mutex_lock(&cachelist->tail->mutex);
                        temptail = cachelist->tail;
                        cachelist->tail->next = newnode;
                        cachelist->tail = cachelist->tail->next;
                        pthread_mutex_unlock(&temptail->mutex);
                    } else {
                        cachelist->tail = newnode;
                        cachelist->head = cachelist->tail;
                        //pthread_mutex_unlock(&mutex_1_Cache);
                    }
                    //pthread_mutex_lock(&mutex_1_Cache);
                    cachelist->leftbytes -= newnode->size;
                    pthread_mutex_unlock(&mutex_1_Cache);
                    pthread_mutex_unlock(&newnode->mutex);

                    printf("File of size %d cached.\n", newnode->size);
                    fflush(stdout);

                    fileLoaded(inode);
                    //pthread_mutex_unlock(&mutex_1_fileLoading);
                    return assignDescriptor(newnode);

                } else {
                    fileLoaded(inode);
                    pthread_mutex_unlock(&mutex_1_fileLoading);
                    FILE *f = fopen(file, "rb");
                    //fflush(stdout);
                    return fileno(f);
                }
            }
        }

    } else
        return -1;
}

/**
 * To find the file in cache. Push it to the end of the queue for (LRU), assign the descriptor and return it.
 * @param inode
 * @return the file descriptor.
 */
int findinCache(ino_t inode) {
    FCB temp = cachelist->head;
    FCB toreturn=NULL;
    if (temp == NULL)
        return 0;
    if (temp != NULL) {
        if (temp->inode == inode && temp->next == NULL) { //if there is only 1 file in cache and it is accessed twice.
            return assignDescriptor(temp);
        } else {
            if (temp->next == NULL)
                return 0;

            while (temp->next != NULL) { //if there are more than 1 files.
                if (temp == cachelist->head && temp != NULL && temp->inode == inode) { //if the file requested is the head of the queue.
                    pthread_mutex_lock(&temp->mutex);
                    pthread_mutex_lock(&cachelist->tail->mutex);
                    FCB temptail = cachelist->tail;
                    cachelist->tail->next = temp;
                    cachelist->head = temp->next;
                    temp->next = NULL;
                    cachelist->tail = cachelist->tail->next;
                    pthread_mutex_unlock(&temp->mutex);
                    pthread_mutex_unlock(&temptail->mutex);

                    toreturn=temp;
                    break;
                } else { //if the file requested is somewhere in middle of the queue.
                    if (temp->next->inode == inode && temp->next->next != NULL) {
                        FCB tempNode = temp->next;
                        pthread_mutex_lock(&temp->mutex);
                        pthread_mutex_lock(&tempNode->mutex);

                        pthread_mutex_lock(&cachelist->tail->mutex);
                        FCB temptail = cachelist->tail;
                        cachelist->tail->next = tempNode;
                        cachelist->tail = tempNode;
                        temp->next = temp->next->next;
                        cachelist->tail->next = NULL;
                        pthread_mutex_unlock(&temptail->mutex);
                        pthread_mutex_unlock(&temp->mutex);
                        pthread_mutex_unlock(&tempNode->mutex);

                        toreturn=tempNode;
                        break;
                    } else {
                        if (temp->next != NULL)
                            temp = temp->next;
                        else
                            return 0;
                    }
                }
            }
            if(toreturn!=NULL)
                return assignDescriptor(toreturn);
            else 
                return 0;
        }
    } else
        return 0;
}

/**
 * To check if the file is already in the cache.
 * @param inode
 * @return 
 */
bool fileInCache(ino_t inode) {
    for (int i = 0; i < 100; i++) {
        if (SWOFT[i] == inode) {
            return true;
        }
    }
    return false;
}

/**
 * Returns the size of the file.
 * @param cfd is the descriptor of the file.
 * @return
 */
int cache_filesize(int cfd) {

    return FileDescriptorTable[cfd - 1]->fcb->size;
}

/**
 * Sending the specified number of bytes to the client.
 * @param cfd
 * @param client
 * @param n
 * @return 
 */
int cache_send(int cfd, int client, int n) {
    char tosend[n];
    if (FileDescriptorTable[cfd - 1] != NULL) {
        char* fileOffset = FileDescriptorTable[cfd - 1]->fcb->fp + FileDescriptorTable[cfd - 1]->offset;

        for (int i = 0; i < n; i++) {
            tosend[i] = *fileOffset;
            fileOffset++;
        }

        FileDescriptorTable[cfd - 1]->offset += n;

        return send(client, tosend, n, 0);
    } else {
        char sendbuff[n]; //buffer to hold the data to be sent.
        read(cfd, sendbuff, n); //Read BUFSIZE no. of bytes.

        return send(client, sendbuff, n, 0); //send the bytes.
    }
}

/**
 * Assigns the Cache descriptor to the FCB from the cache list.
 * @param block
 * @return 
 */
int assignDescriptor(FCB block) {
    int i;
    pthread_mutex_lock(&mutex_3_FDTable);
    for (i = 0; i < 100; i++) {
        if (FileDescriptorTable[i] == NULL) {
            FileDescriptorTable[i] = (struct file_descriptor_table_entry *) malloc(1 * sizeof (struct file_descriptor_table_entry)); //assigning memory to the node.
            FileDescriptorTable[i]->fcb = block;
            FileDescriptorTable[i]->fd = i + 1;
            FileDescriptorTable[i]->offset = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_3_FDTable);

    return i + 1;
}

/**
 * Method for closing the file descriptor.
 * @param cfd
 * @return 
 */
int cache_close(int cfd) {
    if (cfd == -1)
        return -1;
    pthread_mutex_lock(&mutex_3_FDTable);
    if (FileDescriptorTable[cfd - 1]->fd == cfd) {
        FileDescriptorTable[cfd - 1]->fcb = NULL;
        FileDescriptorTable[cfd - 1]->offset = 0;
        free(FileDescriptorTable[cfd - 1]);
        FileDescriptorTable[cfd - 1] = NULL;
    } else {
        close(cfd);
    }
    pthread_mutex_unlock(&mutex_3_FDTable);

    return 1;
}

/**
 * Method to make the entry that the file has started to load.
 * @param inode is the entry in the Filebeingloaded table.
 */
void fileLoad(ino_t inode) {
    pthread_mutex_lock(&mutex_1_fileLoaded);
    for (int i = 0; i < 100; i++) {
        if (FileBeingLoaded[i] == 0) {
            FileBeingLoaded[i] = inode;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_1_fileLoaded);
}

/**
 * Method to check if the file is loading in the cache.
 * @param inode is the inode of the file.
 * @return true if the file is getting loaded.
 */
bool isFileLoading(ino_t inode) {
    pthread_mutex_lock(&mutex_1_fileLoaded);
    for (int i = 0; i < 100; i++) {
        if (inode == FileBeingLoaded[i]) {
            pthread_mutex_unlock(&mutex_1_fileLoaded);
            return true;
        }

    }
    pthread_mutex_unlock(&mutex_1_fileLoaded);
    return false;
}

/**
 * Method to delete the file entry from the being loaded table.
 * @param inode
 */
void fileLoaded(ino_t inode) {
    pthread_mutex_lock(&mutex_1_fileLoaded);
    for (int i = 0; i < 100; i++) {
        if (inode == FileBeingLoaded[i]) {
            FileBeingLoaded[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_1_fileLoaded);
}

/**
 *Method to check if the file is being served so that it does not get evicted from the cache. 
 * @param file is the file being loaded.
 * @return 
 */
bool isBeingServed(FCB file) {
    for (int i = 0; i < 100; i++) {
        if (FileDescriptorTable[i] != NULL) {
            if (FileDescriptorTable[i]->fcb == file)
                return true;
        }
    }
    return false;
}

/**
 * Release the entire FCB memory
 * @param temp is the FCB of the file being released.
 */
void release(FCB temp) {

    free(temp->fp);
    temp->fp = NULL;
    temp->inode = 0;
    pthread_mutex_destroy(&temp->mutex);
    temp->next = NULL;
    temp->size = 0;
    free(temp);
}

/**
 * To make the entry into the SWOFT of all files in the cache.
 * @param inode
 */
void swoftEntry(ino_t inode) {
    pthread_mutex_lock(&mutex_1_SWOFT);
    for (int i = 0; i < 100; i++) {
        if (SWOFT[i] == 0) {
            SWOFT[i] = inode;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_1_SWOFT);
}

/**
 * Method to remove the entry of file from SWOFT if it is evicted.
 * @param inode
 */
void deleteSwoftEntry(ino_t inode) {
    pthread_mutex_lock(&mutex_1_SWOFT);
    for (int i = 0; i < 100; i++) {
        if (SWOFT[i] == inode) {
            SWOFT[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_1_SWOFT);
}
