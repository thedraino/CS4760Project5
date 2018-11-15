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
void incrementClock ( unsigned int shmClock[] );
void printAllocatedResourcesTable();
void printReport();
void terminateIPC();

// Variables to keep statistics over the course of the program run
int totalResourcesRequested;
int totalRequestsGranted;
int totalSafeStateChecks;
int totalResourcesReleased;
int totalProcessesCreated;
int totalProcessesTerminated;

FILE *fp;	// Used for opening and writing to filename described below

/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	
	int i, j;	// Index variables to use in loops
	srand ( time ( NULL ) );	// Seed for OSS to generate random numbers when necessary
	unsigned int newProcessTime[2] = { 0, 0 };	// Initial value for time at which a new process shoudld be created
	totalProcessesCreated = 0;	// Tracks the number of processes that have been created
	int maxRunningProcesses = 18;	// Controls how many processes are allow to be alive at any given time
	int totalProcessLimit = 10;	// Controls how many processes are allowed to be created over the life of the program
	
	/* Output file info */
	int numberOfLines = 0;	// Tracks the number of lines being written to the file
	char logName[10] = "prog.log";	// Name of logfile that will be written to through the program
	fp = fopen ( logName, "w+" );	// Opens file for writing
	
	/* Signal handling */ 
	int killTimer = 2;	// Value to control how many real-life seconds program can run for
	alarm ( killTimer );	// Sets the timer alarm based on value of killTimer

	if ( signal ( SIGINT, handle ) == SIG_ERR ) {
		perror ( "OSS: ctrl-c signal failed." );
	}

	if ( signal ( SIGALRM, handle ) == SIG_ERR ) {
		perror ( "OSS: alarm signal failed." );
	}

	/* Shared memory */
	// Creation of shared memory for simulated clock and block process array 
	shmClockKey = 1993;
	if ( ( shmClockID = shmget ( shmClockKey, ( 2 * ( sizeof ( unsigned int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create shared memory space for simulated clock." );
		return 1;
	}

	shmBlockedKey = 1994;
	if ( ( shmBlockedID = shmget ( shmBlockedKey, ( totalProcessLimit * ( sizeof ( int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
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
	for ( i = 0; i < totalProcessLimit; ++i ) {
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
		totalResourceTable[i] = ( rand() % ( 10 - 1 + 1 ) + 1 ) );
	}

	// Table storing the max claims of each resource for each process.
	// Each row will be updated by on the index of created process upon creation by OSS.
	// Initialize to 0.
	int maxClaimTable[totalProcessLimit][20];
	for ( i = 0; i < totalProcessLimit; ++i ) {
		for ( j = 0; j < 20; ++j ) {
			maxClaimTable[i][j] = 0;
		}
	}

	// Table storing the amount of each resource currently allocated to each process.
	// Updated by OSS whenever resources are granted or released. 
	// Initialize to 0.
	int allocatedTable[totalProcessLimit][20];
	for ( i = 0; i < totalProcessLimit; ++i ) {
		for ( j = 0; j < 20; ++j ) {
			allocatedTable[i][j] = 0;
		}
	}
	
	// Table storing the amount of resources currently available to the system.
	// Updated by OSS whenever resources are granted or released. 
	// (Values are the difference of total resources and currently allocated resources.
	// Initialize as equal to the totalResourceTable
	int availableResourcesTable[20];
	for ( i = 0; i < 20; ++i ) {
		availableResourcesTable[i] = totalResourceTable[i];
	}

	// Table storing the requested resource if the process's request was blocked. 
	// The resource number is stored at the index of the associated process. 
	// OSS stores this value if it is going to put the process in the blocked queue. 
	// OSS resets the value to -1 if the process is unblocked and then granted the resource. 
	int requestedResourceTable[totalProcessLimit];
	for ( i = 0; i < totalProcessLimit; ++i ) {
		requestedResourceTable[i] = -1;
	}
	
	/* Main Loop */
	// Main loop variables
	pid_t pid;
	int processIndex = 0;	// Essentially the same as the process counter variable
	int currentProcesses = 0;	// Counter to track how many processes are currently active
	unsigned int nextRandomProcessTime;
	unsigned int nextProcessTimeBound = 5000;	// Used as a bound when generating the random time for the next process to be created 
	int maxAmountOfEachResource = 3;	// Bound to control the max claim for each resource by USER
	bool timeCheck, processCheck;	// Both flags need to be set to true in order for createProcess to be set to true
	bool createProcess;	// Flag to control whether the logic to create a new process is needed or not
	
	// Main loop will run until the totalProcessLimit has been reached 
	while ( 1 ) {
		createProcess = false;	// Flag is false by default each run through the loop

		// Check first to see if it is time to create a new process
		// If it is, set the flag to true. 
		if ( shmClock[0] > newProcessTime[0] || ( shmClock[0] == newProcessTime[0] && shmClock[1] >= newProcessTime[1] ) ) {
			timeCheck = true;
		} else {
			timeCheck = false;
		}
		
		// Check to see if there system is at its current process limit ( > maxRunningProcesses).
		// If it is not, set the flag to true.
		if ( ( currentProcesses < maxRunningProcesses ) && ( totalProcessesCreated < totalProcessLimit ) ) {
			processCheck = true;
		} else {
			processCheck = false;
		}
		
		// Check to see if timeCheck and processCheck flags are set to true. 
		// If they both are, set the flag to true as well.
		if ( ( timeCheck == true ) && ( processCheck == true ) ) {
			createProcess = true;
		}

		// If the flag gets set to true, continue to create the new process.
		// Randomly create the new USER's max claim vector. Update maxClaimTable for current index.
		// Perform fork and exec passing the process's index and resource vector to USER. 
		if ( createProcess ) {
			processIndex = totalProcessesCreated;	// Sets process index for the various resource tables
			for ( i = 0; i < 20; ++i ) {
				maxClaimTable[processIndex][i] = ( rand() % ( maxAmountOfEachResource - 1 + 1 ) + 1 ); 
			}

			pid = fork();	// Fork the process

			// The fork failed...
			if ( pid < 0 ) {
				perror ( "OSS: Failure to fork child process." );
				kill ( getpid(), SIGINT );
			}

			// In the child process...
			if ( pid == 0 ) {
				// 21 integer buffers to convert process index and resource vector to string.
				// Once converted, all of the buffers will be passed to USER with execl.
				char intBuffer0[3], intBuffer1[3], intBuffer2[3], intBuffer3[3], intBuffer4[3];
				char intBuffer5[3], intBuffer6[3], intBuffer7[3], intBuffer8[3], intBuffer9[3];
				char intBuffer10[3], intBuffer11[3], intBuffer12[3], intBuffer13[3], intBuffer14[3];
				char intBuffer15[3], intBuffer16[3], intBuffer17[3], intBuffer18[3], intBuffer19[3];
				char intBuffer20[3];

				// The buffer number corresponds with that resource in the maxClaimTable.
				sprintf ( intBuffer0, "%d", maxClaimTable[processIndex][0] );	// resource0
				sprintf ( intBuffer1, "%d", maxClaimTable[processIndex][1] );	// resource1
				sprintf ( intBuffer2, "%d", maxClaimTable[processIndex][2] );	// resource2
				sprintf ( intBuffer3, "%d", maxClaimTable[processIndex][3] );	// resource3
				sprintf ( intBuffer4, "%d", maxClaimTable[processIndex][4] );	// resource4
				sprintf ( intBuffer5, "%d", maxClaimTable[processIndex][5] );	// resource5
				sprintf ( intBuffer6, "%d", maxClaimTable[processIndex][6] );	// resource6
				sprintf ( intBuffer7, "%d", maxClaimTable[processIndex][7] );	// resource7
				sprintf ( intBuffer8, "%d", maxClaimTable[processIndex][8] );	// resource8
				sprintf ( intBuffer9, "%d", maxClaimTable[processIndex][9] );	// resource9
				sprintf ( intBuffer10, "%d", maxClaimTable[processIndex][10] );	// resource10
				sprintf ( intBuffer11, "%d", maxClaimTable[processIndex][11] );	// resource11
				sprintf ( intBuffer12, "%d", maxClaimTable[processIndex][12] );	// resource12
				sprintf ( intBuffer13, "%d", maxClaimTable[processIndex][13] );	// resource13
				sprintf ( intBuffer14, "%d", maxClaimTable[processIndex][14] );	// resource14
				sprintf ( intBuffer15, "%d", maxClaimTable[processIndex][15] );	// resource15
				sprintf ( intBuffer16, "%d", maxClaimTable[processIndex][16] );	// resource16
				sprintf ( intBuffer17, "%d", maxClaimTable[processIndex][17] );	// resource17
				sprintf ( intBuffer18, "%d", maxClaimTable[processIndex][18] );	// resource18
				sprintf ( intBuffer19, "%d", maxClaimTable[processIndex][19] );	// resource19
				sprintf ( intBuffer20, "%d", processIndex );	// processIndex

				// Exec to USER passing the appropriate information
				execl ( "./user", "user", intBuffer0, intBuffer1, intBuffer2, intBuffer3,
				       intBuffer4, intBuffer5, intBuffer6, intBuffer7, intBuffer8, 
				       intBuffer9, intBuffer10, intBuffer11, intBuffer12, intBuffer13, 
				       intBuffer14, intBuffer15, intBuffer16, intBuffer17, intBuffer18,
				       intBuffer19, intBuffer20, NULL );

				exit ( 127 );
			} // End of child process logic for OSS

			// In the parent process...
			// Set the time for the next process to be created
			newProcessTime[0] = shmClock[0];
			newProcessTime[1] = shmClock[1];
			nextRandomProcessTime = ( rand() % ( nextProcessTimeBound - 1 + 1 ) + 1;
			newProcessTime[1] += nextRandomProcessTime;
			newProcessTime[0] += newProcessTime[1] / 1000000000;
			newProcessTime[1] = newProcessTime[1] % 1000000000;
			
			// Reset flags that control child creation 
			timeCheck = false;
			processCheck = false;
						 
			// Manage process counters
			currentProcesses++;
			totalProcessesCreated++;
		}
		
		// Check for message...
		// Resource Request Message
						 
		// Resource Release Message
						 
		// Process Termination Message
		
		// Check blocked queue
		
		incrementClock ( shmClock );
						 
	} // End main loop

	// Print program stats
	//printReport();
						 
	// Detach from and delete shared memory segments / message queue
	terminateIPC();

	return 0;
} // End main

/***************************************************************************************************************/
/******************************************* End of Main Function **********************************************/
/***************************************************************************************************************/

// Prints program statistics before the program terminates
void printReport() {
	printf ( "Program Statistics\n" );
	printf ( "\t1. Total processes created: %d\n", totalProcessesCreated );
	printf ( "\t2. Total resource requests: %d\n", totalResourcesRequested );
	printf ( "\t3. Total requests granted: %d\n", totalRequestsGranted );
	printf ( "\t4. Percentage of requests granted: %f\n", ( totalRequestsGranted / totalResourcesRequested ) );
	printf ( "\t5. Total deadlock avoidance algorithm uses: %d\n", totalSafeStateChecks );
	printf ( "\t6. Total Resources released: %d", totalResourcesReleased );
	printf ( "\n" );
}

// Function for signal handling.
// Handles ctrl-c from keyboard or eclipsing 2 real life seconds in run-time.
void handle ( int sig_num ) {
	if ( sig_num == SIGINT || sig_num == SIGALRM ) {
		printf ( "Signal to terminate was received.\n" );
		terminateIPC();
		kill ( 0, SIGKILL );
		wait ( NULL );
		printReport();
		exit ( 0 );
	}
}

// Function that increments the clock by some amount of time at different points. 
// Also makes sure that nanoseconds are converted to seconds when appropriate.
void incrementClock ( unsigned int shmClock[] ) {
	int processingTime = 5000; // Can be changed to adjust how much the clock is incremented.
	shmClock[1] += processingTime;

	shmClock[0] += shmClock[1] / 1000000000;
	shmClock[1] = shmClock[1] % 1000000000;
}

// Function to terminate all shared memory and message queue up completion or to work with signal handling
void terminateIPC() {
	// Close the file
	fclose ( fp );
	
	// Detach from shared memory
	shmdt ( shmClock );
	shmdt ( shmBlocked );

	// Destroy shared memory
	shmctl ( shmClockID, IPC_RMID, NULL );
	shmctl ( shmBlockedID, IPC_RMID, NULL );
	
	// Destroy message queue
	msgctl ( messageID, IPC_RMID, NULL );
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
