// File: user.c | Executable: user
// Created by: Andrew Audrain
//
// USER process that requests and releases resources.
// Generated and managed by OSS. 

#include "oss.h"

int main ( int argc, char *argv[] ) {
	/* General variables */
	int i, j; 			// Loop index variables
	int totalProcessLimit = 10;	// Needs to match the value from OSS
	int myPid = getpid();		// Store process ID for self-identification
	int ossPid = getppid();		// Store parent process ID for sending messages
	int processIndex;		// Store the index passed with exec from OSS. This will always be included
					//   when sending messages to easily find the associated row in the various
					//   resources tables in OSS.
	int maxClaimVector[20];		// Store the max claim vector sent from OSS.
	int allocatedVector[20];	// Store the amount of each resource ( 0-19 ) currently allocated to this USER
	bool iAmBlocked = false;	// Way for process to store locally if it is blocked based off of its value in
					//   shared memory space. 
					
	
	/* Signal handling */
	if ( signal ( SIGINT, handle ) == SIG_ERR ) {
		perror ( "USER: signal failed." );
	}
	
	/* Shared memory */
	// Access shared memory segments
	shmClockKey = 1993;
	if ( ( shmClockID = shmget ( shmClockKey, ( 2 * ( sizeof ( unsigned int ) ) ), 0666 ) ) == -1 ) {
		perror ( "USER: Failure to find shared memory space for simulated clock." );
		return 1;
	}
	
	if ( ( shmClock = (unsigned int *) shmat ( shmClockID, NULL, 0 ) ) < 0 ) {
		perror ( "USER: Failure to attach to shared memory space for simulated clock." );
		return 1;
	}

	shmBlockedKey = 1994;
	if ( ( shmBlockedID = shmget ( shmBlockedKey, ( totalProcessLimit * ( sizeof ( int ) ) ), 0666 ) ) == -1 ) {
		perror ( "USER: Failure to find shared memory space for blocked USER process array." );
		return 1;
	}
	
	if ( ( shmBlocked = (int *) shmat ( shmBlockedID, NULL, 0 ) ) < 0 ) {
		perror ( "USER: Failure to attach to shared memory space for blocked USER process array." );
		return 1;
	}
	
	/* Message queue */
	// Access message queue
	messageKey = 1996;
	if ( ( messageID = msgget ( messageKey, IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "USER: Failure to create the message queue." );
		return 1;
	}
	
	/* Establish USER-specific seed for generating random numbers */
	time_t processSeed; 
	srand ( ( int ) time ( &processSeed ) % getpid() );
	
	/* Constants for determining probability of request, release, or terminate */
	const int probUpper = 100;
	const int probLower = 1;
	const int requestProb = 50;
	const int releaseProb = 40;
	const int terminateProb = 10;
	
	/* Storing of passed arguments from OSS to get process index and max resource claim vector */
	maxClaimVector[0] = atoi ( argv[1] );
	maxClaimVector[1] = atoi ( argv[2] );
	maxClaimVector[2] = atoi ( argv[3] );
	maxClaimVector[3] = atoi ( argv[4] );
	maxClaimVector[4] = atoi ( argv[5] );
	maxClaimVector[5] = atoi ( argv[6] );
	maxClaimVector[6] = atoi ( argv[7] );
	maxClaimVector[7] = atoi ( argv[8] );
	maxClaimVector[8] = atoi ( argv[9] );
	maxClaimVector[9] = atoi ( argv[10] );
	maxClaimVector[10] = atoi ( argv[11] );
	maxClaimVector[11] = atoi ( argv[12] );
	maxClaimVector[12] = atoi ( argv[13] );
	maxClaimVector[13] = atoi ( argv[14] );
	maxClaimVector[14] = atoi ( argv[15] );
	maxClaimVector[15] = atoi ( argv[16] );
	maxClaimVector[16] = atoi ( argv[17] );
	maxClaimVector[17] = atoi ( argv[18] );
	maxClaimVector[18] = atoi ( argv[19] );
	maxClaimVector[19] = atoi ( argv[20] );
	processIndex = atoi ( argv[21] );
	
	//printf ( "Hello, from a %d process.\n", myPid );
	//printf ( "%d: Process %d\n", myPid, processIndex );
	//for ( i = 0; i < 20; ++i ) {
	//	printf ( "R%d: %d ", i, maxClaimVector[i] );
	//}
	//printf ( "\n" );
	
	/* Initialize allocated vector to 0 */
	for ( i = 0; i < 20; ++i ) {
		allocatedVector[i] = 0;
	}
	
	return 0;
}

void handle ( int sig_num ) {
	if ( sig_num == SIGINT ) {
		printf ( "%d: Signal to terminate process was received.\n", getpid() );
		exit ( 0 );
	}
}

