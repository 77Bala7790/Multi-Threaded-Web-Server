# Multi Threaded Web Server

The server is an implementation to handle multiple clients simultaneously and efficiently. 
It implements the following components:
1. Multi Threaded queue for storing the client requests
2. Multi Threaded cache for quick response to similar requests
3. Various types of scheduling for incoming requests.
  * Shortest job first (SJF)
  * First come first serve (FCFS)
  * Round robin (RR)
  * Multi Level queue with feedback(priority wise) (MLFB)

## Pre requisites to run
Any basic c compiler. GCC is preferred.

## How to run
The makefile has all the required commands.
Run the following command in the directory.
  * make all

The above command will compile the whole program.

To run it just type :
either 
  * ./sws

will start the server with default values.
or 
  * ./sws 8080 MLFB 64 9999999
 
8080 -> port to run the server on. It's the default too.
MLFB -> type of scheduler, in this case it's Multi Level queue with feedback. Chose any from above. Their short forms are written after them.
64 -> No of threads to be handled simultaneously
9999999 -> size of cache in bytes.

## Format of the request to the server
The request is in the format: 
  * GET /filename HTTP/1.1

For example:
  * GET /abc.txt HTTP/1.1

## Testing the server
You could simply use telnet to do the job. 
Here are the screenshots for both telnet and server session.



![Alt Text](http://imgur.com/a/mpAYH) 


