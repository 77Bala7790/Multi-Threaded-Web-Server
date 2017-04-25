/*
 * File: sws.c
 * Author: Yogesh Kumar
 * Purpose: This file contains schedulers for the web requests.
 * Date Created: July 1, 2016
 */

#include<stdio.h>
#include "network.h"
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/stat.h>
#include<string.h>
#include "Queue.h"
#include<pthread.h>
#include<stdlib.h>
#include<stdbool.h>
#include "cache.h"

#define BUFSIZE 8192


//All function declarations.
void* SJF(RCB job);
void* AddJob();
bool checkIfInappropriateRequest(RCB job);
void releaseJob(RCB job);
void* Round_Robin(RCB job);
void *MultilevelQueueWithFeedback(RCB job);
void *FCFS();
void *worker_thread();

int SchedulerType; //Variable to find type of scheduler. 1- Shortest job first, 2- Round robin, 3- Multilevel feedback, 4- First come first serve
pthread_t tid[100]; //thread id array.
int port; //port on which the server is to be binded.
Queue queue; //Queue of web requests for worker thread.
Queue ready_queue_8KB; //High priority queue for Multilevel for scheduler.
Queue ready_queue_64KB; //Medium priority queue for Multilevel for scheduler.
bool flag[2]; //Flag array to prevent destructive interference in critical section i.e. no two threads access the shared variable at the same time.
int turn; //Turn variable to prevent destructive interference.
int globalcounter; // The  number of requests being served concurrently.
int numberOfThreads; //number of worker threads permitted.
Queue ready_queue; // Queue of requests for scheduler.
CacheList cachelist;
int cacheSize; //size of cache

//Mutex locks.
pthread_mutex_t mutex_1;
pthread_mutex_t mutex_2;
pthread_mutex_t mutex_3;
pthread_mutex_t mutex_4;

pthread_cond_t cond_worker[100]; //Conditions for each thread. A max of 100 threads.
ConditionQueue queue_suspended; //Queue to keep track of waiting threads.

/*This function opens the socket for incoming connections for the port given by commandLine, waits for incoming connections,
 *serves the request and waits for next one.
 */
int main(int argc, char *argv[]) {
    //default port, if no port is supplied.
    port = 8080;
    //default scheduler is shortest job first.
    SchedulerType = 1;
    numberOfThreads = 100; //default value.

    queue = (struct queue *) malloc(1 * sizeof (struct queue)); //assigning the memory to worker queue.
    ready_queue = (struct queue *) malloc(1 * sizeof (struct queue)); //assigning the memory to ready queue.
    queue_suspended = (struct cond_queue *) malloc(1 * sizeof (struct cond_queue));

    create(ready_queue); //initialization of ready queue.
    create(queue); //initialization of worker queue.
    create_condQ(queue_suspended);

    //cache_init(1048576);
    //cache_init(104);


    globalcounter = 0; //counter for number of requests.

    if (argc == 5) {
        port = atoi(argv[1]);
        numberOfThreads = atoi(argv[3]);
	cacheSize=atoi(argv[4]);
	if(cacheSize==0)
	  cacheSize=1048576; //default cache size value.
	cache_init(cacheSize);
        if (strcmp(argv[2], "SJF") == 0) {
            SchedulerType = 1;
            //fprintf(stderr, "SJF scheduler selected\n");
        }
        if (strcmp(argv[2], "RR") == 0) {
            SchedulerType = 2;
            //fprintf(stderr, "RR scheduler selected\n");
        }
        if (strcmp(argv[2], "MLFB") == 0) {
            ready_queue_8KB = (struct queue *) malloc(1 * sizeof (struct queue)); //initialization of ready queue.
            ready_queue_64KB = (struct queue *) malloc(1 * sizeof (struct queue)); //initialization of ready queue.
            create(ready_queue_8KB);
            create(ready_queue_64KB);
            SchedulerType = 3;
            //fprintf(stderr, "MLFB scheduler selected\n");
        }
        if (strcmp(argv[2], "FCFS") == 0) {
            SchedulerType = 4;
            //fprintf(stderr, "FCFS scheduler selected\n");
        }

    } else {
        //fprintf(stderr, "Default SJF scheduler selected\n");
    }
    for (int i = 0; i < numberOfThreads; i++) { //starting specified number of threads.
        int *arg = malloc(sizeof (*arg));
        *arg = i;
        pthread_create(&(tid[i]), NULL, worker_thread, arg);
    }
    AddJob();

    return 0;

}

/**
 * This method checks the validity of the request and puts the request into ready queue if the request is valid.
 * @return 
 */
void *worker_thread(void* condVariable) {

    while (1) {
        //To avoid conflicts in critical section.
        pthread_mutex_lock(&mutex_1);

        RCB job = deque(queue);
        //Leaving critical section.
        //flag[1] = false;
        pthread_mutex_unlock(&mutex_1);

        if (job != NULL) {
            //if the request was inappropriate, handle it.
            bool appropriateRequest = checkIfInappropriateRequest(job);
            if (!appropriateRequest)//Free the memory allocated to the job if it's not valid.
                releaseJob(job);
            else {

                pthread_mutex_lock(&mutex_2);
                switch (SchedulerType) {
                        //####Critical Section 
                        //Entering critical section.
                        //Adding the job to queue.

                    case 1:enque_SJF(ready_queue, job); //Enqueue for Shortest Job First.
                        break;
                    case 2:enque(ready_queue, job); // Enqueue for round robin.
                        break;
                    case 3: enque(ready_queue_8KB, job); //enqueue for multilevel.
                        break;
                    case 4: enque(ready_queue, job); //enqueue for FCFS.
                        break;
                    default:enque_SJF(ready_queue, job); //Enqueue for Default Shortest Job First.
                        break;
                        //## end critical section
                }
                pthread_mutex_unlock(&mutex_2);
            }
        } else {

            pthread_mutex_lock(&mutex_2);
            //Critical section
            RCB job;
            if (SchedulerType == 3) { //for multilevel scheduler
                job = deque(ready_queue_8KB);

                if (job == NULL) {
                    job = deque(ready_queue_64KB); //if job is null then all jobs in high priority queue have been served, so move to next queue.
                }
                if (job == NULL) {
                    job = deque(ready_queue); ////if job is null then all jobs in high priority and medium priority queue have been served, so move to last queue.
                }
            } else {
                job = deque(ready_queue);
            }

            pthread_mutex_unlock(&mutex_2);
            if (job != NULL) {
                switch (SchedulerType) {
                        //####Critical Section 
                        //Entering critical section.
                        //Adding the job to queue.

                    case 1:SJF(job); //Process the request for Shortest Job First.
                        break;
                    case 2:Round_Robin(job); // Process the request for round robin.
                        break;
                    case 3: MultilevelQueueWithFeedback(job); //enqueue for multilevel.
                        break;
                    case 4: FCFS(job);
                        break;
                    default:SJF(job); //Enqueue for Default Shortest Job First.
                        break;
                        //## end critical section
                }
            } else {
                // There are no requests in worker queue and ready queue, so put the thread to sleep.
                pthread_mutex_lock(&mutex_1);

                int var = *((int*) condVariable);

                //critical section for enqueing in the suspended queue.
                pthread_mutex_lock(&mutex_4);
                enque_Cond(&cond_worker[var], queue_suspended);
                pthread_mutex_unlock(&mutex_4);
                pthread_cond_wait(&cond_worker[var], &mutex_1);
                pthread_mutex_unlock(&mutex_1);
            }
        }
    }
}

/**
 * Method to add the request to the queue.
 * @param port is the port of the server.
 * @return 
 */
void *AddJob() {

    if (port != 0) {
        network_init(port);
        //fprintf(stderr, "binded to port %d\n", port);
        while (1) {
            //wait for the clients to connect.
            network_wait();

            while (globalcounter >= 100) {
            }
            //Get the network descriptor.
            int network = network_open();

            //newnode=NULL;
            //Initializing memory for the job.
            RCB newnode = (struct rcb*) malloc(1 * sizeof (struct rcb));

            //allocating network file descriptor to the request.
            newnode->network = network;

            //Checking if the other thread is already in the critical section.
            pthread_mutex_lock(&mutex_1);

            switch (SchedulerType) {
                    //####Critical Section 
                    //Entering critical section.
                    //Adding the job to queue.

                case 1:enque_SJF(queue, newnode); //Enqueue for Shortest Job First.
                    break;
                case 2:enque(queue, newnode); // Enqueue for round robin.
                    break;
                case 3: enque(queue, newnode); //enqueue for multilevel.
                    break;
                case 4: enque(queue, newnode);
                    break;
                default:enque_SJF(queue, newnode); //Enqueue for Default Shortest Job First.
                    break;
                    //## end critical section
            }
            pthread_mutex_lock(&mutex_3);
            globalcounter++;
            pthread_mutex_unlock(&mutex_3);
            if (globalcounter > 0) {
                //If there are threads waiting, signal them to start running.
                //Critical section for suspended queue, hence mutex lock.
                pthread_mutex_lock(&mutex_4);
                pthread_cond_t* var = DequeCond(queue_suspended);
                if (var != NULL) //if var is not null it means there are threads waiting, if not they are running and will take care of the request.
                    pthread_cond_signal(var);
                pthread_mutex_unlock(&mutex_4);
            }

            //Leaving critical section.
            //flag[2] = false;
            pthread_mutex_unlock(&mutex_1);
        }
    }

    return 0;

}

/**
 * Method for shortest job first scheduler to process the request
 * @param job is the Request control block.
 * @return 
 */
void *SJF(RCB job) {


    //if the request had appropriate request.
    char requestOk[30] = "";
    strcat(requestOk, "HTTP/1.1 200 OK\n\n");
    send(job->network, requestOk, 17, 0);

    //sending the entire file.
    while (job->sent < job->sizeOfFile) {
        int tosend = BUFSIZE;

        if ((job->sizeOfFile - job->sent) < tosend) {
            tosend = job->sizeOfFile - job->sent;
        }
        //        char sendbuff[tosend];
        //        fread(sendbuff, tosend, 1, job->fp);
        //        send(job->network, sendbuff, tosend, 0);
        cache_send(job->fd, job->network, tosend);
        job->sent += tosend;
        printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
        fflush(stdin);
    }
    printf("request for file %s completed.\n", job->requestLines[1]);
    fflush(stdin);

    //Free the memory allocated to the job once it it finished.
    releaseJob(job);

    //decreasing the number of concurrent requests being served by 1.
    pthread_mutex_lock(&mutex_3);
    globalcounter--;
    pthread_mutex_unlock(&mutex_3);

    return 0;

}

/**
 * Method for executing round robin scheduler.
 * @param job is the Request Control Block.
 * @return 
 */
void *Round_Robin(RCB job) {

    //if the request had appropriate request.
    char requestOk[30] = "";
    if (!job->isInSession) {
        strcat(requestOk, "HTTP/1.1 200 OK\n\n");
        send(job->network, requestOk, 17, 0);
    }

    //sending the entire file.
    int quantum = 8192; //quantum of 8KB to be sent in every packet.
    int sent_currentSession = 0; // No of bytes sent in current session.

    job->isInSession = true; //if the job is in session mark it true.

    //send until no. of bytes sent are less than file size and no of bytes sent in current session are less than quantum.
    while (job->sent < job->sizeOfFile && sent_currentSession < quantum) {
        int tosend = BUFSIZE; //Initialize no of bytes to send, to the buffer size.

        if ((job->sizeOfFile - job->sent) < tosend) {//if number of bytes to send is greater than the remaining bytes to be sent,
            tosend = job->sizeOfFile - job->sent; //send remaining bytes.
        }
        //        char sendbuff[tosend]; //buffer to hold the data to be sent.
        //        fread(sendbuff, tosend, 1, job->fp); //Read BUFSIZE no. of bytes.
        //        send(job->network, sendbuff, tosend, 0); //send the bytes.
        cache_send(job->fd, job->network, tosend);
        job->sent += tosend; //Update the number of bytes sent.
        //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
        fflush(stdin);
        sent_currentSession += tosend; //Update the number of bytes sent in current session.
    }

    //If the file was not sent till the quantum limit expired, enqueue it again to be served in future.
    if (job->sent >= job->sizeOfFile) {
        //printf("request for file %s completed.\n", job->requestLines[1]);
        fflush(stdin);
        //If the request has been served free the memory.
        releaseJob(job);

        //decreasing the number of concurrent requests being served by 1.
        pthread_mutex_lock(&mutex_3);
        globalcounter--;
        pthread_mutex_unlock(&mutex_3);
    } else {
        //####Critical Section 
        //Entering critical section.
        //Adding the job to queue.

        pthread_mutex_lock(&mutex_2);

        enque(ready_queue, job);
        //## end critical section

        //Leaving critical section.

        pthread_mutex_unlock(&mutex_2);
    }

    return 0;
}

/**
 *This method is implementing multilevel queue with feedback scheduler.
 */
void *MultilevelQueueWithFeedback(RCB job) {

    if (job != NULL) {


        //if the request had appropriate request.
        char requestOk[30] = "";
        if (!job->isInSession) {
            strcat(requestOk, "HTTP/1.1 200 OK\n\n");
            send(job->network, requestOk, 17, 0);
        }

        //sending the entire file.

        int quantum = 0;

        //Setting the appropriate quantum.
        if (job->level == 1) {
            quantum = 8192; //quantum of 8KB to be sent for every request in high level queue.
            job->level++;
        } else {
            if (job->level == 2) {
                quantum = 65536; //quantum of 64KB to be sent for every request in medium level queue.
                job->level++;
            } else
                quantum = 8192; //quantum of 8KB to be sent for every request in lower priority round robin queue. 

        }
        int sent_currentSession = 0; // No of bytes sent in current session.

        job->isInSession = true; //if the job is in session mark it true.

        //send until no. of bytes sent are less than file size and no of bytes sent in current session are less than quantum.
        while (job->sent < job->sizeOfFile && sent_currentSession < quantum) {
            int tosend = BUFSIZE; //Initialize no of bytes to send, to the buffer size.

            if ((job->sizeOfFile - job->sent) < tosend) {//if number of bytes to send is greater than the remaining bytes to be sent,
                tosend = job->sizeOfFile - job->sent; //send remaining bytes.
            }
            //            char sendbuff[tosend]; //buffer to hold the data to be sent.
            //            fread(sendbuff, tosend, 1, job->fp); //Read BUFSIZE no. of bytes.
            //            send(job->network, sendbuff, tosend, 0); //send the bytes.
            cache_send(job->fd, job->network, tosend);
            job->sent += tosend; //Update the number of bytes sent.
            printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
            fflush(stdin);
            sent_currentSession += tosend; //Update the number of bytes sent in current session.
        }

        //If the file was not sent till the quantum limit expired, enqueue it again to be served in future.
        if (job->sent >= job->sizeOfFile) {
            printf("request for file %s completed.\n", job->requestLines[1]);
            fflush(stdin);
            releaseJob(job);

            //decreasing the number of concurrent requests being served by 1.
            pthread_mutex_lock(&mutex_3);
            globalcounter--;
            pthread_mutex_unlock(&mutex_3);

        } else {
            //####Critical Section 
            //Entering critical section.
            //Adding the job to queue.
            pthread_mutex_lock(&mutex_2);
            if (job->level == 2) { //if job has finished sending 8KB, enqueue it to 64KB queue.
                enque(ready_queue_64KB, job);
            }

            if (job->level == 3) { //if job has finished sending 64 KB, enqueue it to last queue.
                enque(ready_queue, job);
            }
            //## end critical section

            //Leaving critical section.
            pthread_mutex_unlock(&mutex_2);
        }
    }

    return 0;


}

/**
 * This method runs first come first serve scheduler.
 * @return 
 */
void *FCFS(RCB job) {

    //if the request had appropriate request.
    char requestOk[30] = "";
    strcat(requestOk, "HTTP/1.1 200 OK\n\n");
    send(job->network, requestOk, 17, 0);

    //sending the entire file.
    while (job->sent < job->sizeOfFile) {
        int tosend = BUFSIZE;

        if ((job->sizeOfFile - job->sent) < tosend) {
            tosend = job->sizeOfFile - job->sent;
        }
        //        char sendbuff[tosend];
        //        fread(sendbuff, tosend, 1, job->fp);
        //        send(job->network, sendbuff, tosend, 0);
        cache_send(job->fd, job->network, tosend);
        job->sent += tosend;
        //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
        fflush(stdin);
    }
    //printf("request for file %s completed.\n", job->requestLines[1]);
    fflush(stdin);

    //Free the memory allocated to the job once it it finished.
    releaseJob(job);

    //decreasing the number of concurrent requests being served by 1.
    pthread_mutex_lock(&mutex_3);
    globalcounter--;
    pthread_mutex_unlock(&mutex_3);

    return 0;
}

/**
 * This method checks and serves, if the request was inappropriate. It also waits for the client to send the GET request.
 * @param job is the request.
 * @return 
 */
bool checkIfInappropriateRequest(RCB job) {

    //buffer for reading client request.
    char *chunk = (char*) malloc(1024 * sizeof (char));
    //for receiving the request from client.
    recv(job->network, chunk, 1024, 0);


    //breaking the request in appropriate format.
    job->requestLines[0] = strtok(chunk, " "); //request method.
    job->requestLines[1] = strtok(NULL, " "); //request file
    job->requestLines[1] = job->requestLines[1] + 1;
    //job->requestLines[1] = strtok(job->requestLines, " "); //request file
    job->requestLines[2] = strtok(NULL, "\n"); //HTTP version

    //opening the file and assigning the network descriptor to the node.
    //job->fp = fopen(job->requestLines[1], "rb");
    job->fd = cache_open(job->requestLines[1]);
    job->level = 1; // for multilevel scheduler.
    //Getting the file size.
    if (job->fd != -1) {
        struct stat st;
        off_t file_size;
        stat(job->requestLines[1], &st);
        file_size = st.st_size;
        char *size = (char*) malloc(file_size);
        sprintf(size, "%jd", file_size);
        job->sizeOfFile = atol(size);
    } else {
        job->sizeOfFile = 0;
    }
    //printf("Request for file %s admitted.\n", job->requestLines[1]);
    fflush(stdin);
    //Now checking if the request is appropriate,
    bool appropriate = true;
    //if the request had inappropriate request.
    if (strlen(job->requestLines[0]) < 3 || strlen(job->requestLines[1]) < 3 || strlen(job->requestLines[2]) < 8) {
        char wrongrequest[30] = "";
        //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
        fflush(stdin);
        strcat(wrongrequest, "HTTP/1.1 400 Bad request\n\n");
        send(job->network, wrongrequest, 24, 0);
        appropriate = false;
        //printf("request for file %s completed.\n", job->requestLines[1]);
        fflush(stdin);

        //decreasing the number of concurrent requests being served by 1.
        pthread_mutex_lock(&mutex_3);
        globalcounter--;
        pthread_mutex_unlock(&mutex_3);

        return appropriate;
    }

    //To determine the HTTP version of the request. 
    int r1 = strncmp(job->requestLines[2], "HTTP/1.0", 8);
    int r2 = strncmp(job->requestLines[2], "HTTP/1.1", 8);


    if (job->fd == -1 || (r1 != 0 && r2 != 0) || strncmp(job->requestLines[0], "GET", 3) != 0) {
        //if the request had inappropriate HTTP version.
        if ((r1 != 0 && r2 != 0)) {
            char wrongrequest[30] = "";
            //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
            fflush(stdin);
            strcat(wrongrequest, "HTTP/1.1 400 Bad request\n\n");
            send(job->network, wrongrequest, 24, 0);
            appropriate = false;
            //printf("request for file %s completed.\n", job->requestLines[1]);
            fflush(stdin);

            //decreasing the number of concurrent requests being served by 1.
            pthread_mutex_lock(&mutex_3);
            globalcounter--;
            pthread_mutex_unlock(&mutex_3);

            return appropriate;
        }
        //if the request had inappropriate request method.
        if (strncmp(job->requestLines[0], "GET", 3) != 0) {
            char wrongrequest[30] = "";
            //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
            fflush(stdin);
            strcat(wrongrequest, "HTTP/1.1 400 Bad request\n\n");
            send(job->network, wrongrequest, 24, 0);
            appropriate = false;
            //printf("request for file %s completed.\n", job->requestLines[1]);
            fflush(stdin);

            //decreasing the number of concurrent requests being served by 1.
            pthread_mutex_lock(&mutex_3);
            globalcounter--;
            pthread_mutex_unlock(&mutex_3);

            return appropriate;
        }
        //if the request had inappropriate file.
        if (job->fd == -1) {
            char filenotfound[30] = "";
            //printf("Sent %ld bytes of file %s.\n", job->sent, job->requestLines[1]);
            fflush(stdin);
            strcat(filenotfound, "HTTP/1.1 404 File not found\n\n");
            send(job->network, filenotfound, 27, 0);
            appropriate = false;
            //printf("request for file %s completed.\n", job->requestLines[1]);
            fflush(stdin);

            //decreasing the number of concurrent requests being served by 1.
            pthread_mutex_lock(&mutex_3);
            globalcounter--;
            pthread_mutex_unlock(&mutex_3);

            return appropriate;
        }
    }
    //if the request was appropriate.
    return appropriate;
}

/**
 * It releases the memory taken up by the request.
 * @param job is the request.
 */
void releaseJob(RCB job) {
    if (job != NULL) {

        //close the file.
        if (job->fp != NULL)
            fclose(job->fp);

        cache_close(job->fd);
        job->fd = 0;
        job->level = 1;
        job->sent = 0;
        job->seq_no = 0;
        job->sizeOfFile = 0;

        job->requestLines[0] = NULL;
        job->requestLines[1] = NULL;
        job->requestLines[2] = NULL;

        //close the connection.
        close(job->network);
        free(job);
    }
}
