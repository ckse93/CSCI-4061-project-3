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
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_THREADS 100
#define MAX_queue_len 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024

// structs:
typedef struct request_queue {
    int fd;
    char request[BUFF_SIZE];
} request_t;


typedef struct request_array{
    int index;
    request_t *requests;
} request_arr_t;

typedef struct cache_entry{
    /* int len; */
    int index;
    char *request;
    char *content;
}cache_entry_t;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Lock for request_array
pthread_cond_t cv_dispatcher = PTHREAD_COND_INITIALIZER; // Condition variable for dispatcher thread
pthread_cond_t cv_worker = PTHREAD_COND_INITIALIZER; // Condition variable for worker thread
pthread_mutex_t lock_log = PTHREAD_MUTEX_INITIALIZER; // lock for web_log file
pthread_mutex_t lock_cache = PTHREAD_MUTEX_INITIALIZER; //lock for cache

//Gobal Varibales
request_arr_t reqs;
pthread_t *th_dispatcher = NULL;
pthread_t *th_worker = NULL;
char *root_path = NULL;
int *index_worker = NULL;
int qlen;
int cache_entries;
int l_fd;

//====== cache declarations =========/
cache_entry_t * cache_en;
int init_cache_result;
int replace_entry = 0;
int cache_empty_test, cache_full_test;

//
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
// Function to initialize the cache
int initCache(){
    // Allocating memory and initializing the cache array
    if((cache_en = (cache_entry_t*)malloc(cache_entries * sizeof(cache_entry_t))) == NULL){
        fprintf(stderr, "Failed to Malloc for cache.\n");
        return -1;
    }
    return 0;
}

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
    /// return the index if the request is present in the cache
    int i;
    for(i = 0; i < cache_entries; i++){
        if(cache_en[i].request == NULL){
            if(i < cache_entries){
                continue;
            }
            else{
                return -1;
            }
        }
        if(strcmp(cache_en[i].request, request) == 0){
            return i;
        }
    }
    return -1;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory, int memory_size){
    // It should add the request at an index according to the cache replacement policy
    // Make sure to allocate/free memeory when adding or replacing cache entries
    int i,j;
    cache_empty_test = 0;
    cache_full_test = 0;
    for (i = 0 ; i < cache_entries; i++){
        if(cache_en[i].content == NULL && cache_en[i].request == NULL && cache_en[i].index == 0){
            //found slot is empty => insert
            free(cache_en[i].content);
            free(cache_en[i].request);
            cache_en[i].content = malloc(memory_size);
            cache_en[i].request = malloc(strlen(mybuf));
            cache_en[i].index = i;
            cache_en[i].request = mybuf;
            /* cache_en[i].content = (char *)realloc(cache_en[i].content, memory_size * sizeof(char)); */
            cache_en[i].content = memory;
            cache_empty_test = 1000;
            break;
        }
    }
    for(j = 0; j < cache_entries ; j++){
        if(cache_en[i].content != NULL && cache_en[i].request != NULL && cache_en[i].index != 0){
            cache_full_test++;
        }
    }
    if(cache_empty_test != 1000 && cache_full_test == 0){
        //the cache array is full => replace
        free(cache_en[replace_entry].content);
        free(cache_en[replace_entry].request);
        cache_en[replace_entry].index = replace_entry;
        cache_en[replace_entry].request = mybuf;
        /* cache_en[replace_entry].content = (char *)realloc(cache_en[replace_entry].content, memory_size * sizeof(char)); */
        cache_en[replace_entry].content = memory;
        replace_entry = (replace_entry + 1) % cache_entries;
        cache_full_test = 2000;
    }
}

// clear the memory allocated to the cache
void deleteCache(){
    // De-allocate/free the cache memory
    free(cache_en);
}

// Function to open and read the file from the disk into the memory
// Add necessary arguments as needed
int readFromDisk(int fd, char * buf, int size) {
    // Open and read the contents of file given the request
    int nread = read(fd, buf, size);
    if(close(fd) != 0){
        fprintf(stderr, "Error in closing file.\n");
        return -1;
    }
    return nread;
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char * mybuf) {
    // Should return the content type based on the file type in the request
    // (See Section 5 in Project description for more details)
    char *content_type;
    if(strstr(mybuf, ".html") != NULL ||  strstr(mybuf, ".htm") != NULL){
        content_type = "text/html";
    }
    else if (strstr(mybuf, ".jpg") != NULL){
        content_type = "image/jpeg";
    }
    else if (strstr(mybuf, ".gif") != NULL){
        content_type = "image/gif";
    }
    else{
        content_type = "text/plain";
    }
    return content_type;
}

// This function returns the current time in milliseconds
long getCurrentTimeInMicro() {
  struct timeval curr_time;
  gettimeofday(&curr_time, NULL);
  return curr_time.tv_sec * 1000000 + curr_time.tv_usec;
}

/**********************************************************************************/

// Function to receive the request from the client and add to the queue
void * dispatch(void *arg) {
    while (1) {
        // Accept client connection
        int n_fd = accept_connection();
        if(n_fd < 0){
            fprintf(stderr, "FAILED to connect.\n");
            continue;
        }
        // Get request from the client
        char filename[BUFF_SIZE];
        int result = get_request(n_fd, filename);
        if(result != 0){
            fprintf(stderr, "Failed to get_request.\n");
            close(n_fd);
            continue;
        }
        // Add the request into the queue
        if(pthread_mutex_lock(&lock) != 0){
            fprintf(stderr, "Failed to lock.\n");
        }
        while(reqs.index >= qlen){
            if(pthread_cond_wait(&cv_dispatcher, &lock) != 0){
                fprintf(stderr, "Failed to wait.\n");
            }
        }
        reqs.requests[reqs.index].fd = n_fd;
        strcpy(reqs.requests[reqs.index].request,filename);
        reqs.index ++;
        if(pthread_cond_signal(&cv_worker) != 0){
            fprintf(stderr, "Failed to signal.\n");
        }
        if(pthread_mutex_unlock(&lock) != 0){
            fprintf(stderr, "Failed to unlock.\n");
        }
    }
    return NULL;
}


/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
    int count = 0;
    int id = *(int*)arg;
    int len = strlen(root_path);
    char web_log[5*BUFF_SIZE];
    long start, end_here1, end_here2, end_here3, end_here4, timing1, timing2, timing3, timing4;

    while (1) {
        count++;
        char filename[BUFF_SIZE];
        int n_fd;


        // Lock the queue
        if(pthread_mutex_lock(&lock) != 0){
            fprintf(stderr, "Failed to lock.\n");
        }

        /* ////////////////Get the request from the queue////////////////// */
        while(reqs.index <= 0){
            if(pthread_cond_wait(&cv_worker, &lock) != 0){
                fprintf(stderr, "Failed to wait.\n");
            }
        }
        // Start recording time
        start = getCurrentTimeInMicro();
        // get request
        reqs.index = reqs.index -1;
        n_fd = reqs.requests[reqs.index].fd;
        strcpy(filename, reqs.requests[reqs.index].request);
        // signal and unlock
        if(pthread_cond_signal(&cv_worker) != 0){
            fprintf(stderr, "Failed to signal.\n");
        }
        if(pthread_mutex_unlock(&lock) != 0){
            fprintf(stderr, "Failed to unlock.\n");
        }
        /* ///////////////Get the data from the disk or the cache////////////////// */
        char temp[BUFF_SIZE];
        strcpy(temp, root_path);
        int dest_len = len;
        for(;dest_len < BUFF_SIZE && filename[dest_len - len] != '\0'; dest_len++){
            temp[dest_len] = filename[dest_len - len];
        }
        temp[dest_len] = '\0';
        // Check the recived request
        if(access(temp, F_OK) != 0){
            char *error = "File not found.";
            return_error(n_fd, error);
            end_here1 = getCurrentTimeInMicro();
            timing1 = end_here1 - start;
            sprintf(web_log, "[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing1, "MISS");
            printf("[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing1, "MISS");
        }
        else{
            struct stat st;
            stat(temp, &st);
            if(st.st_mode & S_IFREG){
                int size = st.st_size;
                // Open file for reading
                char *buf = (char*)malloc(size * sizeof(char) + 1);
                int fd = open(temp, O_RDONLY);
                if(fd == -1){
                    char *error = "Failed to open file.";
                    return_error(n_fd, error);
                    end_here2 = getCurrentTimeInMicro();
                    timing2 = end_here2 - start;
                    sprintf(web_log, "[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing2, "MISS");
                    printf("[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing2, "MISS");
                }
                else{
                    char *cache_result;
                    int nread;
                    // Read from cache
                    int find_entry = getCacheIndex(filename);
                    if(find_entry != -1){
                        // CACHE HIT
                        cache_result = "HIT";
                        strcpy(buf, cache_en[find_entry].content);
                        nread = size;
                    }
                    else{
                        // CACHE MISS => READ FROM DISK
                        cache_result = "MISS";
                        nread = readFromDisk(fd,buf,size);
                        if(nread == -1){
                            char *error = "Failed to read from Disk.";
                            return_error(n_fd, error);
                            end_here3 = getCurrentTimeInMicro();
                            timing3 = end_here3 - start;
                            sprintf(web_log, "[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing3, "MISS");
                            printf("[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing3, "MISS");
                        }
                        // Add request and content into cache
                        printf("buf %s", buf);
                        addIntoCache(filename,buf,nread);
                    }
                    char * type = getContentType(filename);
                    int temp_return = return_result(n_fd, type, buf, nread);
                    end_here4 = getCurrentTimeInMicro();
                    timing4 = end_here4 - start;
                    if(temp_return != 0){
                        fprintf(stderr, "Failed to return_result.\n");
                        char * error = "Failed to open stream.";
                        sprintf(web_log, "[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing4, "MISS");
                        printf("[%d][%d][%d][%s][%s][%ld ms][%s]\n", id, count, n_fd, filename, error, timing4, "MISS");
                    }
                    else{
                        sprintf(web_log, "[%d][%d][%d][%s][%d][%ld ms][%s]\n", id, count, n_fd, filename, nread, timing4, cache_result);
                        printf("[%d][%d][%d][%s][%d][%ld ms][%s]\n", id, count, n_fd, filename, nread, timing4, cache_result);
                    }
                }
            free(buf);
            }
        }
        /* //////////////Log the request into the file and terminal////////////////// */
        if(pthread_mutex_lock(&lock_log) != 0){
            fprintf(stderr, "Failed to lock web_log.\n");
        }
        if(write(l_fd, web_log, strlen(web_log)) < 0){
            fprintf(stderr, "Failed to write into webserver_log.\n");
        }
        if(pthread_mutex_unlock(&lock_log) != 0){
            fprintf(stderr, "Failed to unlock web_log.\n");
        }
    }
    return NULL;
}
/* ///////Signal handler to free anything when server exit/////////// */
void free_everything(int sig){
    if(reqs.requests != NULL){
        free(reqs.requests);
    }
    if(th_dispatcher != NULL){
        free(th_dispatcher);
    }
    if(th_worker != NULL){
        free(th_worker);
    }
    if(index_worker != NULL){
        free(index_worker);
    }
    deleteCache();
    close(l_fd);
    printf("\n");
    exit(0);
}
/**********************************************************************************/

int main(int argc, char **argv) {
    // Error check on number of arguments
    if(argc != 8){
        printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
        return -1;
    }
    // signal handler to free everything when server exit
    struct sigaction act;
    act.sa_handler = free_everything;
    sigfillset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    // Get the input args
    int port = atoi(argv[1]);
    int num_dispatchers = atoi(argv[3]);
    int num_workers = atoi(argv[4]);
    root_path = argv[2];
    qlen = atoi(argv[6]);
    cache_entries = atoi(argv[7]);
    // Perform error checks on the input arguments
    if(num_dispatchers > MAX_THREADS || num_dispatchers < 1){
        fprintf(stderr,"Number of dispatchers are over 100.\n");
        exit(-1);
    }
    if(num_workers > MAX_THREADS || num_workers < 1){
        fprintf(stderr,"Number of workers are over 100.\n");
        exit(-1);
    }
    if(port < 1024 || port > 65535){
        fprintf(stderr, "Port must be between 1024 and 65535.\n");
        exit(-1);
    }
    if(qlen < 1 || qlen > MAX_queue_len){
        fprintf(stderr, "The length of the queue has to be greater than 0.\n");
        exit(-1);
    }
    if(cache_entries < 1 || cache_entries > MAX_CE){
        fprintf(stderr, "Cache entries must be between 0 and 100.\n");
        exit(-1);
    }
    // Change the current working directory to server root directory
    if(access(root_path, F_OK) != 0){
        printf("Root_path is invalid.\n");
        exit(-1);
    }
    // Start the server and initialize cache
    init(port);
    if((init_cache_result = initCache()) == -1){
        fprintf(stderr, "Failed to initial cache.\n");
    }
    // Remove old webserver_log and open/create the webserver_log file
    if(access("webserver_log",F_OK) == 0){
        remove("webserver_log");
    }
    l_fd = open("webserver_log", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(l_fd == -1){
        fprintf(stderr, " Failed to open the webserver_log file.\n");
    }
    // Print Start Server
    printf("Starting server on port %d: %d disp, %d work\n", port, num_dispatchers, num_workers);
    // Malloc for request array to store it
    reqs.index = 0;
    if((reqs.requests = (request_t *) malloc(qlen * sizeof(request_t))) == NULL){
        fprintf(stderr, "Failed to Malloc for request array.\n");
        exit(-1);
    }
    // Malloc for worker threads and worker threads id
    if((th_worker = (pthread_t *) malloc(num_workers * sizeof(pthread_t))) == NULL){
        fprintf(stderr, "Failed to Malloc for worker_threads.\n");
        exit(-1);
    }
    if((index_worker = (int*)malloc(num_workers * sizeof(int)))== NULL){
        fprintf(stderr, "Failed to Malloc for worker_threads id.\n");
        exit(-1);
    }
    // Malloc for dispatcher threads
    if((th_dispatcher = (pthread_t *) malloc(num_dispatchers * sizeof(pthread_t))) == NULL){
        fprintf(stderr, "Failed to Malloc for dispatcher_threads.\n");
        exit(-1);
    }
    // Create dispatcher and worker threads
    int i,j,h,k;
    for(i = 0; i< num_dispatchers; i++){
        if(pthread_create(&th_dispatcher[i], NULL, dispatch, NULL)){
            fprintf(stderr, "Failed to create dispatchers threads %d\n", i);
        }
    }
    for(j = 0; j <num_workers; j++){
        index_worker[j] = j;
        if(pthread_create(&th_worker[j], NULL, worker, (void *)(index_worker + i))){
            fprintf(stderr, "Failed to create workers threads %d\n", i);
        }
    }
    // Wait for threads to join
    for(h = 0; h < num_dispatchers; h++){
        if(pthread_join(th_dispatcher[h], NULL)){
            fprintf(stderr, "Failed to Wait for dispatchers threads %d\n", h);
        }
    }
    for(k = 0; k < num_workers; k++){
        if(pthread_join(th_worker[k], NULL)){
            fprintf(stderr, "Failed to Wait for workers threads %d\n", k);
        }
    }
    return 0;
}
