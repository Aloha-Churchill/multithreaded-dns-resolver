#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include "array.h"
#include <stdio.h>
#include <arpa/inet.h> //for INET6_ADDRSLEN directive

#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_NAME_LENGTH 255 
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

typedef struct{
    pthread_mutex_t lock; //lock for critical sections
    queue q; //thread safe circular queue

    FILE* serviced_fp; //hostnames that were serviced
    FILE* resolved_fp; //hostnames, IP_addresses that were resolved

    int queue_count; //current number of items in queue
    int file_number; //file index to keep track of which file program is servicing
    int file_number_limit; //total number of files to service
    char* files_to_service[MAX_INPUT_FILES]; //array of filenames to service

} global_info;

void* requester(void* g);

void* resolver(void* g);

#endif