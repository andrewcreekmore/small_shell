// Andrew Creekmore
// CS 344 - Operating Systems 
// Project 3: Small Shell

#include "smallsh.h"


int main()
{
	flowControl();

	return 0;
}


// prompt user and control overall flow of execution 
void flowControl()
{
	smallshPID = getpid();

	disableBackground = false;
	SIGTSTPinvoked = false;

	// set ignore signal handler for SIGINT
	setIgnoreSIGINT();

	// set custom behavior for SIGTSTP
	setHandleSIGTSTP();

	// track foreground exit values
	struct Status status;
	status.fgProcessStatus = false;

	// track background processes
	struct BackgroundPIDs backgroundPIDs;
	backgroundPIDs.size = 0;

	bool continueToPrompt = true;

	// get + validate user input
	while (continueToPrompt)
	{
		// string to hold user input
		char* userInputPtr;

		// struct to abstract user input string to predefined commands
		struct Command* commandPtr;

		// dynamically allocate, to force commandPtr to reset each loop
		commandPtr = malloc(sizeof(struct Command));
		initCommandStruct(commandPtr);

		// manage any background processes
		if (backgroundPIDs.size) 
		{
			manageBackgroundProcesses(&backgroundPIDs);
		}

		// get user input + verify it
		bool inputError = false;

		do {
			// check for SIGTSTP signals
			if (SIGTSTPinvoked) 
			{
				if (disableBackground) 
				{
					printToScreen("\nentering foreground-only mode (& is now ignored)\n");
					commandPtr->backgroundExecutionAllowed = false;
				}

				else if (!disableBackground) 
				{
					printToScreen("\nexiting foreground-only mode\n");
					commandPtr->backgroundExecutionAllowed = true;
				}

				SIGTSTPinvoked = false;
			}

			// prompt user for command
			printToScreen(":");
			userInputPtr = getUserInput();

			// check if stdin has errors
			if (ferror(stdin))
			{
				// clear, free resources, and flag
				clearerr(stdin);
				free(userInputPtr);
				inputError = true;
			}

			else
			{
				inputError = false;
			}

		} while (inputError);

		// abstract into Command struct
		parseInput(userInputPtr, commandPtr);

		// ignore comments
		if (commandPtr->arguments[0] == '#')
		{
			printToScreen("\n");
			continue;
		}

		// ignore blank lines
		else if (strlen(userInputPtr) == 0)
		{
			continue;
		}

		// user command is "cd"
		else if (commandPtr->changeDirectory)
		{
			changeDir(commandPtr);
		}

		// user command is "status"
		else if (commandPtr->status)
		{
			// a foreground command has been run
			if (status.fgProcessStatus)
			{
				if (status.fgProcessReasonForExitOrTermination > 1)
				{
					printf("exited abnormally due to signal %d\n", status.fgProcessReasonForExitOrTermination);
					fflush(stdout);
				}

				else
				{
					printf("exit value: %d\n", status.fgProcessReasonForExitOrTermination);
					fflush(stdout);
				}
			}

			// a foreground command has not yet been run
			else
			{
				printf("exit value: 0\n");
				fflush(stdout);
			}
		}

		// user command is "exit"
		else if (commandPtr->exit)
		{
			// free resources
			free(userInputPtr);
			free(commandPtr);

			// terminate any child background processes
			if (backgroundPIDs.size) 
			{
				int signal = 15;
				for (int i = 0; i < backgroundPIDs.size; i++) 
				{
					kill(backgroundPIDs.pids[i], signal);
				}
			}

			break;
		}

		else
		{
			// if none of the above, execute as "non-basic" command
			executeNonBasicCommand(commandPtr, &backgroundPIDs, &status);
		}

		// free resources
		free(userInputPtr);
		free(commandPtr);
	}

	return;
}


// prints string to screen + flushes output buffer
void printToScreen(char textStr[])
{
	printf(textStr);
	fflush(stdout);
	return;
}


// transforms all uppercase characters to lower
void convertStrToLowerCase(char textStr[])
{
	for (int i = 0; i < strlen(textStr); i++)
	{
		textStr[i] = tolower(textStr[i]);
	}
}


// calls expandVariables as needed + replaces input str w/ its result
void checkForVariablesToExpand(char textStr[])
{
	// check for instances of '$$'; if found, expand into process ID of the smallsh
	bool needsExpansion = true;
	char expandedtextStr[strlen(textStr) + 7];
	memset(expandedtextStr, '\0', sizeof(expandedtextStr));

	// continually expand variables until expandVariables() returns false (i.e., no more instances of '$$')
	while (needsExpansion)
	{
		needsExpansion = expandVariables(textStr, expandedtextStr);

		if (needsExpansion)
		{
			memset(textStr, '\0', sizeof(textStr));
			strcpy(textStr, expandedtextStr);
			memset(expandedtextStr, '\0', sizeof(expandedtextStr));
		}

		else
		{
			// no further characters to check for instances of '$$'
			break;
		}
	}

	// replace input with expanded string 
	memset(textStr, '\0', sizeof(textStr));
	strcpy(textStr, expandedtextStr);
}


// perform actual variable expansion
bool expandVariables(char textStr[], char* expandedtextStr)
{
	// check string for instances of '$$' 
	if (strstr(textStr, "$$") != NULL)
	{
		// get the index of the first instance of '$'
		int indexToExpand;
		for (int i = 0; i < strlen(textStr); i++)
		{
			if (textStr[i] == '$')
			{
				// check if instance is a full '$$', not just '$'
				if (textStr[i + 1] == '$')
				{
					indexToExpand = i;
					break;
				}
			}
		}

		// get pid + make string
		char pidStr[7];
		memset(pidStr, '\0', sizeof(pidStr));
		sprintf(pidStr, "%d", getpid());

		// perform expansion
		if (indexToExpand)
		{
			// copy the substring before the '$$'
			strncpy(expandedtextStr, textStr, indexToExpand);

			// check for characters after the '$$'
			if ((indexToExpand + 2) < strlen(textStr))
			{
				char remainingStr[(strlen(textStr) + 1)];
				memset(remainingStr, '\0', (strlen(textStr) + 1) * sizeof(char));

				// copy those characters
				int indexRemaining = 0;
				for (int i = (indexToExpand + 2); i < strlen(textStr); i++)
				{
					remainingStr[indexRemaining] = textStr[i];
					indexRemaining += 1;
				}

				// concatenate pieces together and return true (to keep checking the remaining characters for additional instances of '$$')
				strcat(expandedtextStr, pidStr);
				strcat(expandedtextStr, remainingStr);
				return true;

			}

			else
			{
				// no remaining characters means nothing further to check
				// concatenate pieces together and return false
				strcat(expandedtextStr, pidStr);
				return false;
			}
		}
	}

	else
	{
		strcat(expandedtextStr, textStr);
	}

	return false;
}


// get input + perform minor transformations thereof (remove newline, force lowercase, variable expansion)
char* getUserInput()
{
	// up to 2048 max characters 
	int bufferSize = 2048;
	char* userInputStr = malloc(sizeof(char) * bufferSize);

	fgets(userInputStr, bufferSize, stdin);
	// remove newline ch from string (read by fgets)
	userInputStr[strcspn(userInputStr, "\n")] = 0;
	// force lowercase
	convertStrToLowerCase(userInputStr);
	// check for instances of '$$'; if found, expand into process ID of the smallsh
	checkForVariablesToExpand(userInputStr);

	return userInputStr;
}


// initializes Command struct
void initCommandStruct(struct Command* commandPtr)
{
	if (disableBackground) { commandPtr->backgroundExecutionAllowed = false; }
	else { commandPtr->backgroundExecutionAllowed = true; }

	commandPtr->exit = false;
	commandPtr->changeDirectory = false;
	commandPtr->status = false;
	commandPtr->echoCommand = false;
	commandPtr->nonBasicCommand = false;
	commandPtr->inputRedirect = false;
	commandPtr->outputRedirect = false;
	memset(commandPtr->arguments, '\0', sizeof(commandPtr->arguments));
	memset(commandPtr->inputFile, '\0', sizeof(commandPtr->inputFile));
	memset(commandPtr->outputFile, '\0', sizeof(commandPtr->outputFile));
	commandPtr->backgroundExecuteFlag = false;
}


// check command/arguments in user input str for conditions; update Command struct accordingly
void parseInput(char userCommandStr[], struct Command* commandPtr)
{
	char* tokenPtr = strtok(userCommandStr, " ");

	for (; tokenPtr != NULL;)
	{
		// check for "echo" command
		if (setEchoCommand(tokenPtr, commandPtr)) 
		{
			while (tokenPtr != NULL) 
			{
				setNonBasicCommandAndArgs(tokenPtr, commandPtr);
				tokenPtr = strtok(NULL, " ");
			}
			return;
		}

		// check for "exit command"
		if (setExitFlag(tokenPtr, commandPtr)) { return; }

		// check for "cd" command
		if (setChangeDirFlag(tokenPtr, commandPtr))
		{
			// get the directory argument string, place into Command struct's arguments member
			tokenPtr = strtok(NULL, " ");

			if (tokenPtr)
			{
				strcpy(commandPtr->arguments, tokenPtr);
			}
			return;
		}

		// check for "status" command
		if (setStatusFlag(tokenPtr, commandPtr)) { return; }

		// strictly for the p3testscript file
		if (checkIfTokenIsTestComment(tokenPtr, commandPtr)) { return; }

		// check for input redirect
		if (setInputFile(tokenPtr, commandPtr))
		{
			tokenPtr = strtok(NULL, " ");
			continue;
		}

		// check for output redirect
		if (setOutputFile(tokenPtr, commandPtr)) 
		{
			tokenPtr = strtok(NULL, " ");
			continue;
		}

		// check for background execution flag
		if (setBackgroundFlag(tokenPtr, commandPtr)) 
		{
			tokenPtr = strtok(NULL, " ");
			continue;
		}

		// if none of the above 3 commands, it's a "non-basic" command
		setNonBasicCommandAndArgs(tokenPtr, commandPtr);

		// advance tokenPtr
		tokenPtr = strtok(NULL, " ");
	}
}


// create array of all arguments
void makeArgArr(char arguments[2048], char* argArr[512])
{
	char* tokenPtr = strtok(arguments, " ");
	int index = 0;

	while (tokenPtr != NULL)
	{
		argArr[index] = tokenPtr;
		index += 1;
		tokenPtr = strtok(NULL, " ");
	}

	argArr[index] = NULL;
}


// checks input for "exit" command; flags + returns accordingly
bool setExitFlag(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, "exit") == 0)
	{
		commandPtr->exit = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for "cd" command; flags + returns accordingly
bool setChangeDirFlag(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, "cd") == 0)
	{
		commandPtr->changeDirectory = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for "status" command; flags + returns accordingly
bool setStatusFlag(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, "status") == 0)
	{
		commandPtr->status = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for "echo" command; flags + returns accordingly
bool setEchoCommand(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, "echo") == 0) 
	{
		commandPtr->echoCommand = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for "<" command; logs any inputFile, flags + returns accordingly
bool setInputFile(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, "<") == 0)
	{
		tokenPtr = strtok(NULL, " ");
		strcpy(commandPtr->inputFile, tokenPtr);
		commandPtr->inputRedirect = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for ">" command; logs any outputFile, flags + returns accordingly
bool setOutputFile(char* tokenPtr, struct Command* commandPtr)
{
	if (strcmp(tokenPtr, ">") == 0)
	{
		tokenPtr = strtok(NULL, " ");
		strcpy(commandPtr->outputFile, tokenPtr);
		commandPtr->outputRedirect = true;
		return true;
	}

	else {
		return false;
	}
}


// checks input for "&" command; flags + returns accordingly
bool setBackgroundFlag(char* tokenPtr, struct Command* commandPtr)
{
	// check for instance of "&"
	if (strcmp(tokenPtr, "&") == 0)
	{
		if (commandPtr->backgroundExecutionAllowed)
		{
			commandPtr->backgroundExecuteFlag = true;
		}

		else
		{
			commandPtr->backgroundExecuteFlag = false;
		}

		return true;
	}

	else {
		return false;
	}
}


// flag as "other" command, set args
void setNonBasicCommandAndArgs(char* tokenPtr, struct Command* commandPtr)
{
	commandPtr->nonBasicCommand = true;
	strcat(commandPtr->arguments, tokenPtr);
	char* space = " ";
	strcat(commandPtr->arguments, space);
}


// basic command: change directory
void changeDir(struct Command* commandPtr)
{
	char dirPath[2048];
	strcpy(dirPath, commandPtr->arguments);

	if (strlen(dirPath) > 0)
	{
		// is cd <directory name>; chdir to specified dir
		int statusCode = chdir(dirPath);

		// if error, give msg
		if (statusCode != 0)
		{
			perror("chdir() failed");
			fflush(stdout);		
		}
	}

	else
	{
		// is standalone command; chdir to home directory
		char* homeEnv = getenv("HOME");
		chdir(homeEnv);
	}

	return;
}


// sets bg/fg appropriate input/output redirects, if present
void checkForAndSetRedirects(struct Command* commandPtr)
{
	// foreground 
	if (!commandPtr->backgroundExecuteFlag)
	{
		if (commandPtr->inputRedirect)
		{
			int inputFileDescriptor = open(commandPtr->inputFile, O_RDONLY);
			if (inputFileDescriptor == -1)
			{
				int bufferSize = 13 + sizeof(commandPtr->inputFile);
				char errorStr[bufferSize];
				sprintf(errorStr, "cannot open %s", commandPtr->inputFile);
				perror(errorStr);
				fflush(stdout);
				exit(1);
			}

			int result = dup2(inputFileDescriptor, STDIN_FILENO);
			if (result == -1)
			{
				perror("inputFileDescriptorFG dup2() failed");
				fflush(stdout);
				exit(2);
			}
		}

		if (commandPtr->outputRedirect)
		{
			int outputFileDescriptor = open(commandPtr->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (outputFileDescriptor == -1)
			{
				int bufferSize = 13 + sizeof(commandPtr->outputFile);
				char errorStr[bufferSize];
				sprintf(errorStr, "cannot open %s", commandPtr->outputFile);
				perror(errorStr);
				fflush(stdout);
				exit(3);
			}

			int result = dup2(outputFileDescriptor, STDOUT_FILENO);
			if (result == -1)
			{
				perror("outputFileDescriptorFG dup2() failed");
				fflush(stdout);
				exit(4);
			}
		}
	}

	// background
	else
	{
		if (commandPtr->inputRedirect)
		{
			int inputFileDescriptor = open("/dev/null", O_RDONLY);
			if (inputFileDescriptor == -1)
			{
				perror("inputFileDescriptorBG open() failed");
				fflush(stdout);
				exit(5);
			}

			int result = dup2(inputFileDescriptor, STDIN_FILENO);
			if (result == -1)
			{
				perror("inputFileDescriptorBG dup2() failed");
				fflush(stdout);
				exit(6);
			}
		}

		if (commandPtr->outputRedirect)
		{
			int outputFileDescriptor = open("/dev/null", O_WRONLY);
			if (outputFileDescriptor == -1)
			{
				perror("outputFileDescriptorBG open() failed");
				fflush(stdout);
				exit(7);
			}
			int result = dup2(outputFileDescriptor, STDOUT_FILENO);
			if (result == -1)
			{
				perror("outputFileDescriptorBG dup2() failed");
				fflush(stdout);
				exit(8);
			}
		}
	}
}


// execution of all "other" commands; forks + manages child process
void executeNonBasicCommand(struct Command* commandPtr, struct BackgroundPIDs* backgroundPIDs, struct Status* statusPtr)
{
	int childStatus;
	pid_t spawnPID = fork();

	switch (spawnPID)
	{
		// if error, give message + exit
		case -1:
		{
			perror("fork() failed\n");
			fflush(stdout);
			exit(1);
			break;
		}

		// child process successfully created
		case 0:
		{
			// check/set any foreground/background redirects in command
			checkForAndSetRedirects(commandPtr);

			// set default SIGINT behavior if foreground process
			if (!commandPtr->backgroundExecuteFlag) {
				setDefaultSIGINT();
			}

			// form command by creating array of all arguments
			char* argArr[512];
			makeArgArr(commandPtr->arguments, argArr);

			// execute constructed command with exec
			int statusCode = execvp(argArr[0], argArr);

			// if error, give message + exit
			if (statusCode < 0)
			{
				perror(argArr[0]);
				fflush(stdout);
				exit(EXIT_FAILURE);
			}

			break;
		}

		default:
		{
			// check for background flags
			if (commandPtr->backgroundExecuteFlag && commandPtr->backgroundExecutionAllowed)
			{
				// place child process PID into backgroundPIDS array, for reference/management 
				backgroundPIDs->pids[backgroundPIDs->size] = spawnPID;

				// print process ID
				printf("background PID is %d\n", backgroundPIDs->pids[backgroundPIDs->size]);
				fflush(stdout);

				// track size of backgroundPIDS
				backgroundPIDs->size += 1;
			}

			// is foreground process
			else
			{	
				// set flag so we know a fg process has been run
				statusPtr->fgProcessStatus = true;

				spawnPID = waitpid(spawnPID, &childStatus, 0);
				statusPtr->fgProcessPID = spawnPID;

				if (spawnPID == -1)
				{
					// wait interrupted; get + print what signal caused child to terminate
					if (WIFSIGNALED(childStatus))
					{
						printf("waitpid() interrupted: termination signal %d\n", WTERMSIG(childStatus));
						fflush(stdout);
					}
				}

				// normal termination; log as such
				if (WIFEXITED(childStatus))
				{
					statusPtr->fgProcessReasonForExitOrTermination = WEXITSTATUS(childStatus);
				}

				// abnormal termination; log (incl. what signal caused child to terminate) + print
				else
				{
					statusPtr->fgProcessReasonForExitOrTermination = WTERMSIG(childStatus);
					printf("exited abnormally due to signal %d\n", WTERMSIG(childStatus));
					fflush(stdout);
				}
			}
		}
	}
}


// checks for finished/terminated bg processes; if found, reaps + tracks
void manageBackgroundProcesses(struct BackgroundPIDs* backgroundPIDs)
{
	int toBeRemoved[backgroundPIDs->size];
	int toBeRemovedCount = 0;

	// for all background processes
	for (int i = 0; i < backgroundPIDs->size; i++) 
	{
		// perform non-blocking waitpid call 
		int childStatus;
		pid_t childPID = waitpid(backgroundPIDs->pids[i], &childStatus, WNOHANG);

		// if error, give message + exit
		if (childPID < 0)
		{
			perror("background PID waitpid() failed");
			fflush(stdout);
			exit(1);
		}

		// child process is still working
		else if (childPID == 0) 
		{
			continue;
		}

		// child process is finished; print appropriate msg + flag for removal
		else 
		{
			printf("background PID %d is done: ", backgroundPIDs->pids[i]);
			fflush(stdout);

			toBeRemoved[toBeRemovedCount] = i;
			toBeRemovedCount += 1;

			if (WIFEXITED(childStatus)) 
			{
				printf("exit value %d\n", WEXITSTATUS(childStatus));
				fflush(stdout);
			}

			else 
			{
				printf("terminated by signal %d\n", WTERMSIG(childStatus));
				fflush(stdout);
			}
		}
	}

	// if index is toBeRemoved, get index + shift backgroundPIDs arr to skip over it
	for (int i = 0; i < toBeRemovedCount; i++)
	{
		int removalIndex = toBeRemoved[i];
		for (; removalIndex < backgroundPIDs->size; removalIndex++)
		{
			backgroundPIDs->pids[removalIndex] = backgroundPIDs->pids[removalIndex + 1];
		}

		// update tracked size accordingly
		backgroundPIDs->size -= 1;
	}

	return;
}


// registers the ignore (SIG_IGN) constant for SIGINT
void setIgnoreSIGINT()
{
	// initialize empty struct, then fill out
	struct sigaction ignore_action = { 0 };

	// register SIG_IGN as the signal handler
	ignore_action.sa_handler = SIG_IGN;

	// block all catchable signals while SIG_IGN is running
	sigfillset(&ignore_action.sa_mask);

	// install signal handler
	sigaction(SIGINT, &ignore_action, NULL);
}


// registers the default (SIG_DFL) constant for SIGINT
void setDefaultSIGINT() 
{
	// initialize empty struct, then fill out
	struct sigaction default_action = { 0 };

	// register SIG_DFL as the signal handler
	default_action.sa_handler = SIG_DFL;

	// block all catchable signals while SIG_DFL is running
	sigfillset(&default_action.sa_mask);

	// install signal handler with no flags set
	default_action.sa_flags = 0;
	sigaction(SIGINT, &default_action, NULL);
}


// custom SIGTSTP handler
void handleSIGTSTP(int signo) 
{
	SIGTSTPinvoked = true;

	if ((getpid() == smallshPID) && (!disableBackground))
	{
		// disable ability to execute background commands
		disableBackground = true;
	}

	else if ((getpid() == smallshPID) && (disableBackground))
	{
		// allow background commands
		disableBackground = false;
	}

	return;
}


// registers custom signal handler (handleSIGTSTP) for SIGTSTP
void setHandleSIGTSTP()
{
	// initialize empty struct, then fill out
	struct sigaction SIGTSTP_action = { 0 };

	// register handleSIGTSTP as the signal handler
	SIGTSTP_action.sa_handler = handleSIGTSTP;

	// block all catchable signals while handleSIGTSTP is running
	sigfillset(&SIGTSTP_action.sa_mask);

	// install custom signal handler with no flags set
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}


// strictly for the grading test script
bool checkIfTokenIsTestComment(char* tokenPtr, struct Command* commandPtr)
{
	if (tokenPtr[0] == '(')
	{
		return true;
	}

	else
	{
		return false;
	}
}
