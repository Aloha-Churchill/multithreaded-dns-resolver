//TODO
//protect stderr
//rigorous testing --> Test edge cases especially
//add comments
//valgrind testing

#include "multi-lookup.h"
#include "util.h" //for dns lookup
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h> //for timing of function
#include <string.h>

//insert hostnames onto thread safe shared circular queue
void* requester(void* g){
    global_info* g_struct = (global_info*)g;
    queue* q = &(g_struct->q);

    int num_files_serviced = 0;

    //opeen 1 file at a time, file number is shared across threads
    pthread_mutex_lock(&(g_struct->lock));
    while(g_struct->file_number_limit > g_struct->file_number){
        FILE* fp = fopen(g_struct->files_to_service[g_struct->file_number], "r");
        g_struct->file_number ++;
    pthread_mutex_unlock(&(g_struct->lock));

        //handle case if fp cannot find filename
        if(fp == NULL){
            pthread_mutex_lock(&(g_struct->lock));
            fprintf(stderr, "Invalid file %s\n", g_struct->files_to_service[g_struct->file_number]);
            g_struct->file_number ++;
            pthread_mutex_unlock(&(g_struct->lock));

        }
        else{

            //store in char buffer that takes in maximum of MAX_NAME_LENGTH     
            char line[MAX_NAME_LENGTH];
            while(fgets(line, MAX_NAME_LENGTH, fp)){
                if(array_produce(q, line) != 0){
                    pthread_mutex_lock(&(g_struct->lock));
                    fprintf(stderr, "Could not insert into circular queue \n");
                    array_free(&(g_struct->q));
                    free(g_struct);
                    exit(-1);
                    pthread_mutex_unlock(&(g_struct->lock));
                }



                pthread_mutex_lock(&(g_struct->lock));
                fputs(line, g_struct->serviced_fp);
                g_struct->queue_count += 1;
                pthread_mutex_unlock(&(g_struct->lock));
            }

            fclose(fp);
            num_files_serviced ++;

        }
    }

    printf("Thread (%lu) serviced %i files.\n", pthread_self(), num_files_serviced);
    pthread_exit(0);

}


//removes hostnames from shared circular queue and then uses  dnslookup to match to IP Address 
void* resolver(void* g){
    global_info* g_struct = (global_info*)g;
    queue* q = &(g_struct->q);

    int num_hostnames_resolved = 0;
    while(1){
        char str_to_resolve[MAX_NAME_LENGTH];

        //calling insert method of circular queue and handling error case and other case
        if(array_consume(q, str_to_resolve, MAX_NAME_LENGTH) != 0){
            pthread_mutex_lock(&(g_struct->lock));
            fprintf(stderr, "Could not remove from circular queue\n");
            array_free(&(g_struct->q));
            free(g_struct);
            exit(-1);
            pthread_mutex_unlock(&(g_struct->lock));
        }

        int len = strlen(str_to_resolve);
        if(str_to_resolve[len-1] == '\n'){
            str_to_resolve[len-1] = '\0';
        }
        
        //checking if poison pill to then exit all consumer threads
        if(strcmp(str_to_resolve,"poisonpill") == 0){
            //re-insert poisonpill into queue to terminate other threads and then terminate current thread
            if(array_produce(q, "poisonpill") != 0){
                pthread_mutex_lock(&(g_struct->lock));
                fprintf(stderr, "Could not insert poison  pill into circular queue \n");
                array_free(&(g_struct->q));
                free(g_struct);
                exit(-1);
                pthread_mutex_unlock(&(g_struct->lock));
            }

            //is printf thread safe?
            printf("Thread (%lu) resolved %i hostnames.\n", pthread_self(), num_hostnames_resolved);
            pthread_exit(0);
        }

        else{
            pthread_mutex_lock(&(g_struct->lock));
            g_struct->queue_count --;
            pthread_mutex_unlock(&(g_struct->lock));

            char IP_address[MAX_IP_LENGTH];
            if(dnslookup(str_to_resolve, IP_address, MAX_IP_LENGTH) != -1){
                num_hostnames_resolved ++;

                pthread_mutex_lock(&(g_struct->lock));
                fputs(str_to_resolve, g_struct->resolved_fp);
                fputs(",", g_struct->resolved_fp);
                fputs(IP_address, g_struct->resolved_fp);
                fputs("\n", g_struct->resolved_fp);
                pthread_mutex_unlock(&(g_struct->lock));

            }
            else{

                pthread_mutex_lock(&(g_struct->lock));
                fputs(str_to_resolve, g_struct->resolved_fp);
                fputs(", ", g_struct->resolved_fp);
                fputs("NOT_RESOLVED", g_struct->resolved_fp);
                fputs("\n", g_struct->resolved_fp);
                pthread_mutex_unlock(&(g_struct->lock));

            }
        }
    }
}


int main(int argc, char* argv[]){
    struct timeval start;
    struct timeval end;

    gettimeofday(&start, NULL);

    //validating the input
    if(argc <= 5){
        printf("Not enough arguments\n");
        printf("Synopsis: multi-lookup <# requester> <# resolver> <requester log> <resolver log> [<data file> ...]\n");
        exit(-1);
    }
    if(argc > MAX_INPUT_FILES + 5){
        fprintf(stderr, "Too many input files\n");
        exit(-1);
    }

    int num_requesters = atoi(argv[1]);
    int num_resolvers = atoi(argv[2]);

    if(num_requesters > MAX_REQUESTER_THREADS || num_resolvers > MAX_RESOLVER_THREADS){
        fprintf(stderr, "Too many requester or resolver threads (or both)\n");
        exit(-1);
    }
    if(num_requesters <= 0 || num_resolvers <= 0){
        fprintf(stderr, "No requester or resolver threads\n");
        exit(-1);
    }

    //initializing shared information

    global_info* g = (global_info*)malloc(sizeof(global_info));

    //checking serviced.txt and resolved.txt
    g->serviced_fp = fopen(argv[3], "w");
    g->resolved_fp = fopen(argv[4], "w");
    if(g->serviced_fp == NULL || g->resolved_fp == NULL){
        fprintf(stderr, "Failed to open or create file\n");
        exit(-1);
    }

    //initializing shared circular queue
    if(array_init(&(g->q)) != 0){
        printf("Initializing shared array failed\n");
        exit(-1);
    }

    pthread_mutex_init(&(g->lock), NULL);
    g->queue_count = 0;

    for(int i = 5; i < argc; i++){
        g->files_to_service[i-5] = argv[i];
    }
    g->file_number = 0;
    g->file_number_limit = argc-5;



    //creating the threads
    pthread_t requester_arr[num_requesters];
    pthread_t resolver_arr[num_resolvers];

    for(int i = 0; i < num_requesters; i++){
        if(pthread_create((pthread_t*)&requester_arr[i], NULL, &requester, g) != 0){
            printf("thread creation failed\n");
            exit(-1);
        }
    }
    for(int i = 0; i < num_resolvers; i++){
        if(pthread_create((pthread_t*)&resolver_arr[i], NULL, &resolver, g) != 0){
            printf("thread creation failed\n");
            exit(-1);
        }
    }

    //joining back threads
    for(int j = 0; j < num_requesters; j++){
        if(pthread_join(requester_arr[j], NULL) != 0){
            printf("thread join failed\n");
            exit(-1);
        }
    }

    //put poison pill here, after all requesters have terminated
    if(array_produce(&(g->q), "poisonpill") != 0){
        pthread_mutex_lock(&(g->lock));
        fprintf(stderr, "Could not insert poison pill into circular queue \n");
        array_free(&(g->q));
        free(g);
        exit(-1);
        pthread_mutex_unlock(&(g->lock));
    }
    
    pthread_mutex_lock(&(g->lock));
    g->queue_count += 1;
    pthread_mutex_unlock(&(g->lock));

    for(int j = 0; j < num_resolvers; j++){
        if(pthread_join(resolver_arr[j], NULL) != 0){
            printf("thread join failed\n");
            exit(-1);
        }
    }

    fclose(g->serviced_fp);
    fclose(g->resolved_fp);

    //freeing memory
    array_free(&(g->q));
    free(g);

    gettimeofday(&end, NULL);
    float total_time = (end.tv_sec - start.tv_sec) + 1e-6*(end.tv_usec - start.tv_usec);
    printf("Program is done running -- TIME TAKEN: %0.8f seconds\n", total_time);

    return 0;
}