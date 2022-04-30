// Andrew Creekmore
// CS 344 - Assignment 3: smallsh
// Winter '22

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


// struct for storing command + argument info
struct Command
{
	bool exit;
	bool changeDirectory;
	bool status;
	bool echoCommand;
	bool nonBasicCommand;
	bool inputRedirect;
	bool outputRedirect;
	char arguments[2048];
	char inputFile[256];
	char outputFile[256];
	bool backgroundExecuteFlag;
	bool backgroundExecutionAllowed;
};


// struct for storing foreground process status info
struct Status 
{
	pid_t fgProcessPID;
	bool fgProcessStatus;
	int fgProcessReasonForExitOrTermination;
};


// struct for storing backgroundPID info
struct BackgroundPIDs 
{
	int size;
	pid_t pids[200];	// max number of user processes (of any kind) on os1
};


pid_t smallshPID;
bool disableBackground;
bool SIGTSTPinvoked;
void flowControl();
void printToScreen(char textStr[]);
void convertStrToLowerCase(char textStr[]);
void checkForVariablesToExpand(char textStr[]);
bool expandVariables(char textStr[], char* expandedtextStr);
char* getUserInput();
void initCommandStruct(struct Command* commandPtr);
void parseInput(char userCommandStr[], struct Command* commandPtr);
void makeArgArr(char arguments[2048], char* argArr[512]);
bool setExitFlag(char* tokenPtr, struct Command* commandPtr);
bool setChangeDirFlag(char* tokenPtr, struct Command* commandPtr);
bool setStatusFlag(char* tokenPtr, struct Command* commandPtr);
bool setInputFile(char* tokenPtr, struct Command* commandPtr);
bool setOutputFile(char* tokenPtr, struct Command* commandPtr);
bool setBackgroundFlag(char* tokenPtr, struct Command* commandPtr);
bool setEchoCommand(char* tokenPtr, struct Command* commandPtr);
void setNonBasicCommandAndArgs(char* tokenPtr, struct Command* commandPtr);
void changeDir(struct Command* commandPtr);
void checkForAndSetRedirects(struct Command* commandPtr);
void executeNonBasicCommand(struct Command* commandPtr, struct BackgroundPIDs* backgroundPIDs, struct Status* statusPtr);
void manageBackgroundProcesses(struct BackgroundPIDs* backgroundPIDs);
void setIgnoreSIGINT();
void setDefaultSIGINT();
void handleSIGTSTP(int signo);
void setHandleSIGTSTP();
bool checkIfTokenIsTestComment(char* tokenPtr, struct Command* commandPtr);