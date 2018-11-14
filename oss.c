// File: oss.c | Executable (after make): oss
// Created by: Andrew Audrain
//
// Master process to simulate a resource management module

#include "oss.h"

// Queue code is gotten from https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
// A structure to represent a queue
typedef struct {
	int front, rear, size;
	unsigned capacity;
	int *array;  
	int pid;
} Queue;
                   
// Queue Protypte Functions
Queue* createQueue ( unsigned capacity );
int isFull ( Queue* queue ); 
int isEmpty ( Queue* queue );
void enqueue ( Queue* queue, int item );
int dequeue ( Queue* queue );
int front ( Queue* queue );
int rear ( Queue* queue );

// Other Prototype Functions
void calculateNeed ( int need[maxProcesses][maxResources], int maximum[maxProcesses][maxResources], int allot[maxProcesses][maxResources] );
void isSafeState ( int processes[], int available[], int maximum[][maxResources], int allot[][maxResources] );
void nanosecondConverter ( unsigned int shmClock[] );
void printAllocatedResourcesTable();
void terminateIPCObjects();

// Variables to keep statistics over the course of the program run
int totalResourcesRequested;
int totalRequestsGranted;
int totalSafeStateChecks;
int totalResourcesReleased;
int totalProcessesTerminated;

/**********************************************/
/*************** Main Function ****************/
/**********************************************/
int main ( int argc, char *argv[] ) {
	printf ( "Hello, from OSS.\n" );

	return 0;
}

// Function to create a queue of given capacity.
// It initializes size of queue as 0.
Queue* createQueue ( unsigned capacity ) {
	Queue* queue = (Queue*) malloc ( sizeof ( Queue ) );
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;	// This is important, see the enqueue
	queue->array = (int*) malloc ( queue->capacity * sizeof ( int ) );

	return queue;
}

// Queue is full when size becomes equal to the capacity
int isFull ( Queue* queue ) {
	return ( queue->size == queue->capacity );
}

// Queue is empty when size is 0
int isEmpty ( Queue* queue ) { 
	return ( queue->size == 0 );
}

// Function to add an item to the queue.
// It changes rear and size.
void enqueue ( Queue* queue, int item ) {
	if ( isFull ( queue ) ) 
		return;
	
	queue->rear = ( queue->rear + 1 ) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
	printf ( "%d enqueued to queue.\n", item );
}

// Function to remove an item from queue.
// It changes front and size.
int dequeue ( Queue* queue ) {
	if ( isEmpty ( queue ) )
		return INT_MIN;

	int item = queue->array[queue->front];
	queue->front = ( queue->front + 1 ) % queue->capacity;
	queue->size = queue->size - 1;

	return item;
}

// Function to get front of queue.
int front ( Queue* queue ) {
	if ( isEmpty ( queue ) )
		return INT_MIN;

	return queue->array[queue->front];
}

// Function to get rear of queue.
int rear ( Queue* queue ) {
	if ( isEmpty ( queue ) )
		return INT_MIN;

	return queue->array[queue->rear];
}
