#include "array.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// initialize circular array and other variables in struct
int array_init(queue *a){
	int i = 0;
	while(i < ARRAY_SIZE){
		a->array[i] = (char*)malloc(MAX_NAME_LENGTH);
		memset(a->array[i], '\0', MAX_NAME_LENGTH);
		i += 1;
	}

	a->insert_pos = 0;
	a->remove_pos = 0;

	//initializing semaphores
	if(sem_init(&(a->mutex), 0, 1) == -1 || sem_init(&(a->num_spaces), 0, 10) == -1 || sem_init(&(a->num_items), 0, 0) == -1){
		printf("Semaphore initialization failed\n");
		return -1;
	}

	return 0;

}


int array_produce(queue *a, char* input){

	//note -- possibly dont need these semaphores because covering in test.c
	sem_wait(&(a->num_spaces));
	sem_wait(&(a->mutex));

	//----CODE FOR ADDING TO BUFFER
	int input_length = strlen(input);

	if(input_length + 1 > MAX_NAME_LENGTH){
		printf("Input name exceed maximum character limit\n");
		return -1;
	}

	memset(a->array[a->insert_pos], '\0', MAX_NAME_LENGTH);
	if(strncpy(a->array[a->insert_pos], input, MAX_NAME_LENGTH) == NULL){
		printf("Could not copy into queue\n");
		return -1;
	}
	
	//update insert pos
	if(a->insert_pos == ARRAY_SIZE-1){
		a->insert_pos = 0;
	}
	else{
		a->insert_pos += 1;
	}

	//----END CODE FOR ADDING TO BUFFER

	sem_post(&(a->mutex));
	sem_post(&(a->num_items));

	return 0;

}

int array_consume(queue *a, char* output, int output_length){

	sem_wait(&(a->num_items));
	sem_wait(&(a->mutex));

	//----CODE FOR TAKING FROM BUFFER
	if(output_length > MAX_NAME_LENGTH){
		printf("Consumer buffer exceeds output length, changing output length to size of input\n");
		output_length = MAX_NAME_LENGTH;
	}

	if(strncpy(output, a->array[a->remove_pos], output_length) == NULL){
		printf("Could not consume from queue\n");
		return -1;
	}

	if(a->remove_pos == ARRAY_SIZE-1){
		a->remove_pos  = 0;
	}
	else{
		a->remove_pos += 1;
	}
	
	//----END CODE FOR TAKING FROM BUFFER

	sem_post(&(a->mutex));
	sem_post(&(a->num_spaces));
	return 0;
}

void array_free(queue *a){
	
	sem_destroy(&(a->mutex));
	sem_destroy(&(a->num_items));
	sem_destroy(&(a->num_spaces));

	int i = 0;
	while(i < ARRAY_SIZE){
		free(a->array[i]);
		i += 1;
	}
}