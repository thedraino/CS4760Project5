// File: user.c | Executable: user
// Created by: Andrew Audrain
//
// USER process that requests and releases resources.
// Generated and managed by OSS. 

#include "oss.h"

int main ( int argc, char *argv[] ) {
	/* General variables */
	int totalProcessLimit = 10;	// Needs to match the value from OSS
	int myPid = getpid();		// Store process ID for self-identification
	int ossPid = getppid();		// Store parent process ID for sending messages
	int processIndex;		// Store the index passed with exec from OSS. This will always be included
					//   when sending messages to easily find the associated row in the various
					//   resources tables in OSS.
	int maxClaimVector[20];		// Store the max claim vector sent from OSS.
	int allocatedVector[20];	// Store the amount of each resource ( 0-19 ) currently allocated to this USER
	
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
	
	printf ( "Hello, from a %d process.\n", getpid() );

	
	return 0;
}

void handle ( int sig_num ) {
	if ( sig_num == SIGINT ) {
		printf ( "%d: Signal to terminate process was received.\n", getpid() );
		exit ( 0 );
	}
}

