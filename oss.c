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
bool isSafeState ( int available[], int maximum[][maxResources], int allot[][maxResources] );
void incrementClock ( unsigned int shmClock[] );
void printAllocatedResourcesTable( int num1, int array[][maxResources] );
void printMaxClaimTable( int num1, int array[][maxResources] );
void printReport();
void terminateIPC();

// Variables to keep statistics over the course of the program run
int totalResourcesRequested;
int totalRequestsGranted;
int totalSafeStateChecks;
int totalResourcesReleased;
int totalProcessesCreated;
int totalProcessesTerminated;

const int maxRunningProcesses = 18;	// Controls how many processes are allow to be alive at any given time
const int totalProcessLimit = 100;	// Controls how many processes are allowed to be created over the life of the program
const int maxAmountOfEachResource = 4;	// Bound to control the max claim for each resource by USER
FILE *fp;	// Used for opening and writing to filename described below

/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	
	int i, j;	// Index variables to use in loops
	srand ( time ( NULL ) );	// Seed for OSS to generate random numbers when necessary
	unsigned int newProcessTime[2] = { 0, 0 };	// Initial value for time at which a new process shoudld be created
	totalProcessesCreated = 0;	// Tracks the number of processes that have been created
	int myPid = getpid();
	
	/* Output file info */
	int numberOfLines = 0;	// Tracks the number of lines being written to the file
	char logName[10] = "prog.log";	// Name of logfile that will be written to through the program
	fp = fopen ( logName, "w+" );	// Opens file for writing
	
	/* Signal handling */ 
	const int killTimer = 2;	// Value to control how many real-life seconds program can run for
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
		perror ( "OSS: Failure to attach to shared memory space for blocked USER process array." );
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
		totalResourceTable[i] = ( rand() % ( 10 - 1 + 1 ) + 1 );
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
	bool timeCheck, processCheck;	// Both flags need to be set to true in order for createProcess to be set to true
	bool createProcess;	// Flag to control whether the logic to create a new process is needed or not
	
	Queue* blockedQueue = createQueue ( totalProcessLimit );
	
	// Variables used when handling received messages
	int tempPid;
	int tempIndex;
	int tempRequest;
	int tempRelease;
	bool tempTerminate;
	bool tempGranted;
	unsigned int tempClock[2];
	unsigned int receivedTime[2];
	int tempHolder; 
	
	// Main loop will run until the totalProcessLimit has been reached 
	while ( 1 ) {
		
		// Check the number of lines in the logfile after the most recent run through the loop.
		// Terminate if logfile exceeds 10000 lines (per project instruction ).
		if ( numberOfLines >= 10000 ) {
			fprintf ( fp, "OSS: Logfile has exceeded it's maximum length. Program terminating...\n" );
			kill ( getpid(), SIGINT );
		}
		
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
			
			fprintf ( fp, "Max Claim Vector for new newly generated process: Process %d\n", processIndex);
			for ( i = 0; i < 20; ++i ) {
				fprintf ( fp, "%d: %d\t", i, maxClaimTable[processIndex][i] );
			}
			fprintf ( fp, "\n" );
			
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

				fprintf ( fp, "OSS: Process %d (PID: %d) was created at %d:%d.\n", processIndex, 
					 getpid(), shmClock[0], shmClock[1] );
				numberOfLines++;
				
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
			nextRandomProcessTime = ( rand() % ( nextProcessTimeBound - 1 + 1 ) + 1 );
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
		msgrcv ( messageID, &message, sizeof( message ), 5, IPC_NOWAIT );
		
		// Set variables based on received message...
		tempPid = message.pid;
		tempIndex = message.tableIndex;
		tempRequest = message.request;
		tempRelease = message.release;
		tempTerminate = message.terminate;
		tempGranted = message.resourceGranted;
		tempClock[0] = message.messageTime[0];
		tempClock[1] = message.messageTime[1];
		
		// Resource Request Message
		if ( tempRequest != -1 ) {
			fprintf ( fp, "OSS: Process %d requested Resource %d at %d:%d.\n", tempIndex, 
				 tempRequest, tempClock[0], tempClock[1] );
			numberOfLines++;
			totalResourcesRequested++;
			
			// Store that requested resource value in the request vector for that process.
			requestedResourceTable[tempIndex] = tempRequest;
			
			// Temporarily change the resource tables to test the state
			allocatedTable[tempIndex][tempRequest]++;
			availableResourcesTable[tempRequest]--;
			
			// Run banker's algorithm...
			// If the state is safe, send the USER a message granting the resource request.
			// Update the tables.
			if (isSafeState ( availableResourcesTable, maxClaimTable, allocatedTable ) ) {
				totalRequestsGranted++;
				message.msg_type = tempIndex;
				message.pid = getpid();
				message.tableIndex = tempIndex;
				message.request = -1;
				message.release = -1;
				message.terminate = false;
				message.resourceGranted = true;
				message.messageTime[0] = shmClock[0];
				message.messageTime[1] = shmClock[1];
				
				if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
					perror ( "OSS: Failure to send message." );
				}
				
				fprintf ( fp, "OSS: Process %d was granted its request of Resource %d at %d:%d.\n", 
					 tempIndex, tempRequest, shmClock[0], shmClock[1] );
				numberOfLines++;				
			}
			// if it's unsafe, block the user in shm and add to queue
			// update logfile
			else {
				// Reset tables to their state before the test
				allocatedTable[tempIndex][tempRequest]--;
				availableResourcesTable[tempRequest]++;
				
				// Place that process's index in the blocked queue
				enqueue ( blockedQueue, tempIndex );
				
				// Set the blocked process flag in shared memory for USER to see
				shmBlocked[tempIndex] = 1;
				
				fprintf ( fp, "OSS: Process %d was denied its request of Resource %d and was blocked at %d:%d.\n", 
					 tempIndex, tempRequest, shmClock[0], shmClock[1] );
				numberOfLines++;
			}
			
			incrementClock ( shmClock );
		}
		
		// Resource Release Message
		if ( tempRelease != -1 ) {
			fprintf ( fp, "OSS: Process %d indicated that it was releasing some of Resource %d at %d:%d.\n", 
				 tempIndex, tempRelease, tempClock[0], tempClock[1] );
			numberOfLines++;
			
			totalResourcesReleased++;
			allocatedTable[tempIndex][tempRelease]--;
			availableResourcesTable[tempRelease]++;
	
			fprintf ( fp, "OSS: Process %d release notification was handled at %d:%d.\n", tempIndex, shmClock[0],
				 shmClock[1] );
			numberOfLines++;
			incrementClock ( shmClock );
		}
		
		// Process Termination Message
		if ( tempTerminate == true ) {
			fprintf ( fp, "OSS: Process %d terminated at %d:%d.\n", tempIndex, tempClock[0], tempClock[1] );
			numberOfLines++;
			
			for ( i = 0; i < 20; ++i ) {
				tempHolder = allocatedTable[tempIndex][i];
				allocatedTable[tempIndex][i] = 0;
				availableResourcesTable[i] += tempHolder;
			}
			currentProcesses--;
			
			fprintf ( fp, "OSS: Process %ds termination notification was handled at %d:%d.\n", tempIndex, 
				 shmClock[0], shmClock[1] );
			numberOfLines++;
			incrementClock ( shmClock );
		}
		
		// Check blocked queue
		if ( !isEmpty ) {
			// Dequeue the next item from the queue, store its index and find the
			//   requested resource which had it caused it to get blocked.
			tempIndex = dequeue ( blockedQueue );
			tempRequest = requestedResourceTable[tempIndex]; 
			totalResourcesRequested++;
			
			// Temporarily change the resource tables to test the state
			allocatedTable[tempIndex][tempRequest]++;
			availableResourcesTable[tempRequest]--;
			
			// Run banker's algorithm...
			// If the state is safe, send the USER a message granting the resource request.
			// Update the tables.
			if (isSafeState ( availableResourcesTable, maxClaimTable, allocatedTable ) ) {
				totalRequestsGranted++;
				message.msg_type = tempIndex;
				message.pid = getpid();
				message.tableIndex = tempIndex;
				message.request = -1;
				message.release = -1;
				message.terminate = false;
				message.resourceGranted = true;
				message.messageTime[0] = shmClock[0];
				message.messageTime[1] = shmClock[1];
				
				if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
					perror ( "OSS: Failure to send message." );
				}
				
				// Set the blocked process flag in shared memory for USER to see
				shmBlocked[tempIndex] = 0;
				
				fprintf ( fp, "OSS: Process %d was granted its request of Resource %d at %d:%d.\n", 
					 tempIndex, tempRequest, shmClock[0], shmClock[1] );
				numberOfLines++;				
			} else {
				// Reset tables to their state before the test
				allocatedTable[tempIndex][tempRequest]--;
				availableResourcesTable[tempRequest]++;
				
				// Place that process's index in the blocked queue
				enqueue ( blockedQueue, tempIndex );
				
				// Set the blocked process flag in shared memory for USER to see
				shmBlocked[tempIndex] = 1;
				
				fprintf ( fp, "OSS: Process %d was denied it's request of Resource %d and was blocked at %d:%d.\n", 
					 tempIndex, tempRequest, shmClock[0], shmClock[1] );
				numberOfLines++;
			}
			incrementClock ( shmClock );
		}
		
		incrementClock ( shmClock );
		
		if ( numberOfLines % 20 == 0 ) {
			//printAllocatedResourcesTable( totalProcessesCreated, allocatedTable );
			fprintf ( fp, "Currently Allocated Resources\n" );
			fprintf ( fp, "\tR0\tR1\tR2\tR3\tR4\tR5\tR6\tR7\tR8\tR9\tR10\tR11\tR12\tR13\tR14\tR15\tR16\tR17\tR18\tR19\n" );
			for ( i = 0; i < totalProcessesCreated; ++i ) {
				fprintf ( fp, "P%d:\t", i );
				for ( j = 0; j < 20; ++j ) {
					fprintf ( fp, "%d\t", allocatedTable[i][j] );
				}
				fprintf ( fp, "\n" );
			}
		}			
			
	} // End main loop

	// Print program stats
	printReport();
						 
	// Detach from and delete shared memory segments / message queue
	terminateIPC();

	return 0;
} // End main

/***************************************************************************************************************/
/******************************************* End of Main Function **********************************************/
/***************************************************************************************************************/

// Generates the values for the ResourcesNeeded matrix used in banker's algorithm (isSafeState()).
// Function to find the need of each process in the system.
void calculateNeed ( int need[maxProcesses][maxResources], int maximum[maxProcesses][maxResources], int allot[maxProcesses][maxResources] ) {
	int i, j; 
	for ( i = 0; i < maxProcesses; ++i ) {
		for ( j = 0; j < maxResources; ++j ) {
			need[i][j] = maximum[i][j] - allot[i][j];
		}
	}
}

// Code adapted from a c++ version of the algorithm at https://www.geeksforgeeks.org/program-bankers-algorithm-set-1-safety-algorithm/
// Adaptation of banker's algorithm to handle deadlock avoidance for oss. 
bool isSafeState ( int available[], int maximum[][maxResources], int allot[][maxResources] ) {
	totalSafeStateChecks++;
	int index; 
	
	int need[maxProcesses][maxResources];
	calculateNeed ( need, maximum, allot );	// Function to calculate need matrix
	
	bool finish[maxProcesses] = { 0 };
	
	// Make a copy of the available resources vector.
	int work[maxResources];
	int i; 
	for ( i = 0; i < maxResources; ++i ) {
		work[i] = available[i]; 
	}
	
	int count = 0; 
	// Loop runs while all processes are not finished or system is not in a safe state
	while ( count < maxProcesses ) {
		int p; 
		bool found = false;
		for ( p = 0; p < maxProcesses; ++p ) {
			if ( finish[p] == 0 ) {
				int j;
				for ( j = 0; j < maxResources; ++j ) {
					if ( need[p][j] > work[j] )
					    break;
				}
				if ( j == maxResources ) {
					int k;
					for ( k = 0; k < maxResources; ++k ) {
						work[k] += allot[p][k];
					}
					finish[p] = 1;
					found = true;
				}
			}
		}
		
		if ( found == false ) {
			//return true;
			return false;
		}    
	}
	
	return true; 
	
}

// Prints program statistics before the program terminates
void printReport() {
	double approvalPercentage = totalRequestsGranted / totalResourcesRequested;
	printf ( "Program Statistics\n" );
	fprintf ( fp, "Program Statistics\n" );
	printf ( "\t1. Total processes created: %d\n", totalProcessesCreated );
	fprintf ( fp, "\t1. Total processes created: %d\n", totalProcessesCreated );
	printf ( "\t2. Total resource requests: %d\n", totalResourcesRequested );
	fprintf ( fp, "\t2. Total resource requests: %d\n", totalResourcesRequested );
	printf ( "\t3. Total requests granted: %d\n", totalRequestsGranted );
	fprintf ( fp, "\t3. Total requests granted: %d\n", totalRequestsGranted );
	printf ( "\t4. Percentage of requests granted: %f\n", approvalPercentage );
	fprintf ( fp, "\t4. Percentage of requests granted: %f\n", approvalPercentage );
	printf ( "\t5. Total deadlock avoidance algorithm uses: %d\n", totalSafeStateChecks );
	fprintf ( fp, "\t5. Total deadlock avoidance algorithm uses: %d\n", totalSafeStateChecks );
	printf ( "\t6. Total Resources released: %d", totalResourcesReleased );
	fprintf ( fp, "\t6. Total Resources released: %d", totalResourcesReleased );
	printf ( "\n" );
	fprintf ( fp, "\n" );
}

// Function for signal handling.
// Handles ctrl-c from keyboard or eclipsing 2 real life seconds in run-time.
void handle ( int sig_num ) {
	if ( sig_num == SIGINT || sig_num == SIGALRM ) {
		printf ( "Signal to terminate was received.\n" );
		printReport();
		terminateIPC();
		kill ( 0, SIGKILL );
		wait ( NULL );
		exit ( 0 );
	}
}

// Function print the table showing all currently allocated resources
void printAllocatedResourcesTable( int num1, int array[][maxResources] ) {
	int i, j;
	num1 = totalProcessesCreated;
	
	printf ( "Currently Allocated Resources\n" );
	printf ( "\tR0\tR1\tR2\tR3\tR4\tR5\tR6\tR7\tR8\tR9\tR10\tR11\tR12\tR13\tR14\tR15\tR16\tR17\tR18\tR19\n" );
	for ( i = 0; i < totalProcessesCreated; ++i ) {
		printf ( "P%d:\t", i );
		for ( j = 0; j < 20; ++j ) {
			printf ( "%d\t", array[i][j] );
		}
		printf ( "\n" );
	}
}

// Function to print the table showing the max claim vectors for each process
void printMaxClaimTable( int num1, int array[][maxResources] ){
	int i, j;
	num1 = totalProcessesCreated; 
	
	printf ( "Max Claim Table\n" );
	printf ( "\tR0\tR1\tR2\tR3\tR4\tR5\tR6\tR7\tR8\tR9\tR10\tR11\tR12\tR13\tR14\tR15\tR16\tR17\tR18\tR19\n" );
	for ( i = 0; i < totalProcessesCreated; ++i ) {
		printf ( "P%d:\t", i );
		for ( j = 0; j < 20; ++j ) {
			printf ( "%d\t", array[i][j] );
		}
		printf ( "\n" );
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
