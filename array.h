#ifndef ARRAY_H
#define ARRAY_H

//take semaphores out
#include <semaphore.h>

#define ARRAY_SIZE 10
#define MAX_NAME_LENGTH 255


typedef struct {
	char* array[ARRAY_SIZE];
	int insert_pos;
	int remove_pos;
	
	sem_t mutex;
	sem_t num_items;
	sem_t num_spaces;

} queue;

int array_init(queue *a);
int array_produce(queue *a, char* input);
int array_consume(queue *a,  char* output, int output_length); //store in output
void array_free(queue *a);

#endif
