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

/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	
	int i; 
	int numberOfLines = 0;	// Tracks the number of lines being written to the file
	srand ( time ( NULL ) );	// Seed for OSS to generate random numbers when necessary
	char fileName[10] = "prog5.log";	// Name of logfile that will be written to through the program
	

	// Creation of shared memory for simulated clock and block process array
	shmClockKey = 1993;
	if ( ( shmClockID = shmget ( shmClockKey, ( 2 * ( sizeof ( unsigned int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create shared memory space for simulated clock." );
		return 1;
	}

	shmBlockedKey = 1994;
	if ( ( shmBlockedID = shmget ( shmBlockedKey, ( 100 * ( sizeof ( int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create shared memory space for blocked USER process array." );
		return 1;
	}
	
	// Attach to and initialize shared memory for clock and blocked process array
	if ( ( shmClock = (unsigned int *) shmat ( shmClockID, NULL, 0 ) ) < 0 ) {
		perror ( "OSS: Failure to attach to shared memory space for simulated clock." );
		return 1;
	}
	shmClock[0] = 0; // Will hold the seconds value for the simulated clock
	shmClock[1] = 0; // Will hold the nanoseconds value for the simulated clock

	if ( ( shmBlocked = (int *) shmat ( shmBlockedID, NULL, 0 ) ) < 0 ) {
		perror ( "OSS: Failure to attach to shared memory space for simulated clock." );
		return 1;
	}
	// This table has index to correspond with each USER process. 
	// If the process is ever blocked by OSS, that flag at its corresponding index
	//  will get flipped to 1 to indicate that it is blocked. 
	// The flag will be flipped back to 0 once it is no longer blocked and has been
	//  granted its requested resource.
	for ( i = 0; i < 100; ++i ) {
		shmBlocked[i] = 0;
	}
	
	// Creation of message queue
	messageKey = 1996;
	if ( ( messageID = msgget ( messageKey, IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create the message queue." );
		return 1;
	}


	/* Creation of different data tables */

	// Table storing the total resources in the system.
	// Number of each resource is a random number between 1-10 (inclusive).
	int totalResourceTable[20]; 
	for ( i = 0; i < 20; ++i ) {
		totalResourceTable[i] = ( rand() % ( 10 - 1 + 1 ) + 1 );
	}

	// Table storing the max claims of each resource for each process.
	// Each row will be updated by on the index of created process upon creation by OSS.
	int maxClaimTable[100][20];

	// Table storing the amount of each resource currently allocated to each process.
	// Updated by OSS whenever resources are granted or released. 
	int allocatedTable[100][20];

	// Table storing the amount of resources currently available to the system.
	// Updated by OSS whenever resources are granted or released. 
	// (Values are the difference of total resources and currently allocated resources.
	int availableResourcesTable[100][20];

	// Table storing the requested resource if the process's request was blocked. 
	// The resource number is stored at the index of the associated process. 
	// OSS stores this value if it is going to put the process in the blocked queue. 
	// OSS resets the value to -1 if the process is unblocked and then granted the resource. 
	int requestedResourceTable[100];
	for ( i = 0; i < 100; ++i ) {
		requestedResourceTable[i] = -1;
	}

	// Detach from and Delete shared memory segments / message queue
	shmdt ( shmClock );
	shmdt ( shmBlocked );
	
	msgctl ( messageID, IPC_RMID, NULL ); 
	shmctl ( shmClockID, IPC_RMID, NULL );
	shmctl ( shmBlockedID, IPC_RMID, NULL );

	return 0;
}

/***************************************************************************************************************/
/******************************************* End of Main Function **********************************************/
/***************************************************************************************************************/

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
