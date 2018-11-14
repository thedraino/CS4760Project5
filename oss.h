// File: oss.h
// Created by: Andrew Audrain
//
// Header file to be used with oss.c and user.c

#ifndef OSS_HEADER_FILE
#define OSS_HEADER_FILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>

/* Macros */
#define maxProcesses 18
#define maxResources 20

/* Structure(s) */
// Structure used in the message queue 
typedef struct {
	long msg_type;		// Controls who can receive the message.
	int pid;		// Store the pid of the sender.
	int tableIndex;		// Store the index of the child process from USER.
	int request;		// Some value from 0-19 if the child process is requesting a resource from OSS.
	int release;		// Some value from 0-19 if the child is notifying OSS that it is releasing a resource.
	bool terminate;		// Default is false. Gets changed to true when child terminates. 
	bool resourceGranted;	// Default is false. Gets changed to true when OSS approves the resource request from USER. 
} Message;

/* Function Prototypes */
void handle ( int sig_num );	// Function to handle the alarm or ctrl-c signals

/* Message Queue Variables */
Message message;
int messageID;
key_t messageKey;

/* Shared Memory Variables */
int shmClockID;
int *shmClock;
key_t shmClockKey;

int shmBlockedID;
int *shmBlocked;
key_t shmBlockedKey;

#endif 
