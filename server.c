#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"
#include <stdbool.h>
#include <unistd.h>

#define MAX_THREADS 100
#define MAX_queue_len 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024

/* Gobal Declaration */
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t dispatchers_COND = PTHREAD_COND_INITIALIZER;
pthread_cond_t workers_COND = PTHREAD_COND_INITIALIZER;
char root_path[BUFF_SIZE];

/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGESSTION. FEEL FREE TO MODIFY AS NEEDED
*/

// structs:
typedef struct request_queue {
   int fd;
   void *request;
} request_t;

typedef struct cache_entry {
    int len;
    char *request;
    char *content;
} cache_entry_t;

/* ************************ Dynamic Pool Code ***********************************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  while(1) {
    // Run at regular intervals
    // Increase / decrease dynamically based on your policy
  }
}
/**********************************************************************************/

/* ************************************ Cache Code ********************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /// return the index if the request is present in the cache
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  // It should add the request at an index according to the cache replacement policy
  // Make sure to allocate/free memeory when adding or replacing cache entries
}

// clear the memory allocated to the cache
void deleteCache(){
  // De-allocate/free the cache memory
}

// Function to initialize the cache
void initCache(){
  // Allocating memory and initializing the cache array
}

// Function to open and read the file from the disk into the memory
// Add necessary arguments as needed
int readFromDisk(/*necessary arguments*/) {
  // Open and read the contents of file given the request
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char * mybuf) {
  // Should return the content type based on the file type in the request
  // (See Section 5 in Project description for more details)
}

// This function returns the current time in milliseconds
int getCurrentTimeInMills() {
  struct timeval curr_time;
  gettimeofday(&curr_time, NULL);
  return curr_time.tv_usec;
}

/**********************************************************************************/

// Function to receive the request from the client and add to the queue
void * dispatch(void *arg) {

  while (1) {

    // Accept client connection

    // Get request from the client

    // Add the request into the queue

   }
   return NULL;
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  int time_start, time_end, time_total, getRequest;
  char* filename = *(char*)arg;                           //not sure. 

   while (1) {
     pthread_mutex_lock (&queue_lock);
     time_start = getCurrentTimeInMills();                // Start recording time
     getRequest = get_request(request_t.fd);
    if (get_request(int fd, char *filename)) {
      /* code */
    }                                                      // Get the request from the queue

                                                          // Get the data from the disk or the cache

    time_end = getCurrentTimeInMills();                 // Stop recording the time
    time_total = time_end - time_start;
    pthread_mutex_unlock (&queue_lock);

                                                          // Log the request into the file and terminal

                                                          // return the result
  }
  return NULL;
}

/**********************************************************************************/

int main(int argc, char **argv) {
    // Error check on number of arguments
    if(argc != 8){
        printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
        return -1;
    }
    // Get the input args
    int port = atoi(argv[1]);
    int num_dispatchers = atoi(argv[3]);
    int num_workers = atoi(argv[4]);
    strcpy(root_path, argv[2]);


    // Perform error checks on the input arguments
    if(num_dispatchers > MAX_THREADS){
        fprintf(stderr,"Number of dispatchers are over 100.\n");
        exit(-1);
    }
    if(num_workers > MAX_THREADS){
        fprintf(stderr,"Number of workers are over 100.\n");
        exit(-1);
    }
    if(port < 1024 || port > 65535){
        fprintf(stderr, "Port must be between 1024 and 65535.\n");
        exit(-1);
    }
    // Change the current working directory to server root directory
    if(chdir(root_path)){
        printf("Failed to chdir\n");
        exit(-1);
    }
    // Start the server and initialize cache
    printf("Starting server on port %d: %d disp, %d work\n", port, num_dispatchers , num_workers);
    init(port);
    //TODO :initCache();
    // Create dispatcher and worker threads
    int i,j,h,k;
    pthread_t dispatchers[MAX_THREADS];
    pthread_t workers[MAX_THREADS];
    for(i = 0 ; i < num_dispatchers ; i++){
        if(pthread_create(&dispatchers[i], NULL, &dispatch, NULL)){
            fprintf(stderr, "Failed to create dispatchers threads %d\n", i);
            exit(-1);
        }
    }
    for(j = 0 ; j < num_workers ; j++){
        int temp = j;
        if(pthread_create(&workers[j], NULL, &worker, (void *)&temp)){
            fprintf(stderr, "Failed to create workers threads %d\n", j);
            exit(-1);
        }
    }
    //Wait for Threads
    for(h = 0; h < num_dispatchers; h++){
        if(pthread_join(dispatchers[h], NULL)){
            fprintf(stderr, "Failed to Wait for dispatchers threads %d\n", h);
        }
    }
    for(k = 0; k < num_workers; k++){
        if(pthread_join(workers[k], NULL)){
            fprintf(stderr, "Failed to Wait for workers threads %d\n", k);
        }
    }
    // TODO: clean up
    return 0;
}
