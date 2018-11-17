
// File: user.c | Executable: user
// Created by: Andrew Audrain
//
// USER process that requests and releases resources.
// Generated and managed by OSS. 

#include "oss.h"

bool hasResourcesToRelease ( int arr[] );
bool canRequestMore ( int arr1[], int arr2[] );

int main ( int argc, char *argv[] ) {
	/* General variables */
	int i, j; 			// Loop index variables
	int totalProcessLimit = 100;	// Needs to match the value from OSS
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
	// Values can be tuned to get desired output
	const int probUpper = 100;
	const int probLower = 1;
	const int requestProb = 100;
	const int releaseProb = 55;
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
	
	/* Variables for main loop */
	bool waitingOnRequest = false;	// Flag to prevent USER from requesting another resource 
					//   while still waiting on a previous request.
	int randomAction;	// Will store the random number to decide what action to take
	int selectedResource;	// Will store the resource that USER wants to request or release
	bool validResource;	// Flag to indicate if the resource is okay to request or release
	
	// Enter main loop
	while ( 1 ) {
		
		// If the process is not in the blocked queue in OSS (indicated by shared memory) and 
		//   the process is not waiting on OSS to respond to a resource request
		if ( shmBlocked[processIndex] == 0 && waitingOnRequest == false ) { 
			
			// Check to see if it still needs to resources. If has been allocated enough resources
			//   to match the max claim vector, then it can do it task and terminate.
			if ( !canRequestMore ( maxClaimVector, allocatedVector ) ) {
				// Set message outgoing message information and send message
				message.msg_type = 5;
				message.pid = myPid;
				message.tableIndex = processIndex;
				message.request = 0;
				message.release = 0;
				message.terminate = true;
				message.resourceGranted = false;
				message.messageTime[0] = shmClock[0];
				message.messageTime[1] = shmClock[1];
				    
				if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
					perror ( "USER: Failure to send message." );
				}	
				
				//printf ( "Process %d -- PID: %d -- Terminated at %d:%d\n", processIndex, myPid, 
				//	message.messageTime[0], message.messageTime[1] );
				
				exit ( EXIT_SUCCESS );
			}
			
			// If process can still request resources, continue...
			randomAction = ( rand() % ( probUpper - probLower + 1 ) + probLower );
			
			// Request Resource
			if ( randomAction > releaseProb && randomAction <= requestProb ) {
				validResource = false;
				
				// Check to make sure the selected resource isn't already maxed out. If it is, 
				//   select a different resource to request.
				while ( validResource == false ) {
					selectedResource = ( rand() % ( 19 - 0 + 1 ) + 0 );
					if ( allocatedVector[selectedResource] < maxClaimVector[selectedResource] ) {
						validResource = true;
					} 
				} // End of select a valid resource loop
				
				// Set message outgoing message information and send message
				message.msg_type = 5;
				message.pid = myPid;
				message.tableIndex = processIndex;
				message.request = selectedResource;
				message.release = -1;
				message.terminate = false;
				message.resourceGranted = false;
				message.messageTime[0] = shmClock[0];
				message.messageTime[1] = shmClock[1];
					    
				if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
					perror ( "USER: Failure to send message." );
				}
				
				// Flag can only be set back to false later if a received message indicates that the
				//   above request was granted by OSS. 
				waitingOnRequest = true;
			} // End of request resource 
			
			// Release Resource
			if ( randomAction > terminateProb && randomAction <= releaseProb ) {
				validResource = false;
				
				// Check to make sure that USER currently has resources to release.
				// If it does, check to make sure it selects a valid resource.
				if ( hasResourcesToRelease ( allocatedVector ) ) {
					while ( validResource == false ) {
						selectedResource = ( rand() % ( 19 - 0 + 1 ) + 0 );
						if ( allocatedVector[selectedResource] > 0 ) {
							validResource = true;
						} 
					} // End of selected a valid resource loop
					
					// Set message outgoing message information and send message
					message.msg_type = 5;
					message.pid = myPid;
					message.tableIndex = processIndex;
					message.request = -1;
					message.release = selectedResource;
					message.terminate = false;
					message.resourceGranted = false;
					message.messageTime[0] = shmClock[0];
					message.messageTime[1] = shmClock[1];
					    
					if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
						perror ( "USER: Failure to send message." );
					}	
					
					allocatedVector[selectedResource]--;
				} // End of select a valid resource loop	    		    
			} // End of release resource
			
			// Terminate
			if ( randomAction > 0 && randomAction <= terminateProb ) {
				// Set message outgoing message information and send message
				message.msg_type = 5;
				message.pid = myPid;
				message.tableIndex = processIndex;
				message.request = -1;
				message.release = -1;
				message.terminate = true;
				message.resourceGranted = false;
				message.messageTime[0] = shmClock[0];
				message.messageTime[1] = shmClock[1];
				    
				if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
					perror ( "USER: Failure to send message." );
				}	
				
				//printf ( "Process %d -- PID: %d -- Terminated at %d:%d\n", processIndex, myPid, 
				//	message.messageTime[0], message.messageTime[1] );
				
				exit ( EXIT_SUCCESS );
			} // End of terminate resource
		} // End of action loop
		
		// Check to see if OSS has responded to any resource requests for this process
		msgrcv ( messageID, &message, sizeof ( message ), myPid, IPC_NOWAIT );
				    
		// If a resource request was granted by OSS, reset waitingOnRequest flag and increase
		//   the amount of that resource in the allocated resource vector.
		if ( message.resourceGranted == true ) {
			waitingOnRequest = false;
			allocatedVector[selectedResource]++;
		}
	
	} // End of main loop
	
	return 0;
}

void handle ( int sig_num ) {
	if ( sig_num == SIGINT ) {
		printf ( "%d: Signal to terminate process was received.\n", getpid() );
		exit ( 0 );
	}
}

// Returns true is allocated resource vector has less resources in total than the max claim vector allows
bool canRequestMore ( int arr1[], int arr2[] ) {
	int i;
	int sum1 = 0;
	int sum2 = 0;
	
	for ( i = 0; i < 20; ++i ) {
		sum1 += arr1[i];	// Total resources in max claim vector
		sum2 += arr2[i];	// Total resources in allocated resource vector
	}
	
	if ( sum2 < sum1 ) 
		return true;
	else
		return false;
}
				    
// Returns true if allocated resource vector isn't empty
bool hasResourcesToRelease ( int arr[] ) {
	int i;
	int sum = 0;
	for ( i = 0; i < 20; ++i ) {
		sum += arr[i];
	}
	
	if ( sum > 0 ) 
		return true;
	else
		return false;
}
	
