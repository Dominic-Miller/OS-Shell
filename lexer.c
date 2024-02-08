#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


// Some globally defined maximums for our functions
#define MAX_HISTORY 3
#define MAX_COMMAND_LENGTH 200
#define MAX_JOBS 10

// Global arrays to manage command history and background jobs
char command_history[MAX_HISTORY][MAX_COMMAND_LENGTH];
int history_index = 0;

pid_t background_jobs[MAX_JOBS];
char background_commands[MAX_JOBS][MAX_COMMAND_LENGTH];
// Tracks the number of active background jobs
int active_jobs = 0; 

//------------------------------------------------------------------------------------//

// Main function
int main()
{
	// Initialize command history and background jobs storage
    memset(command_history, 0, sizeof(command_history));
    memset(background_jobs, 0, sizeof(background_jobs));

	job_num = 1;
	// Infinite loop for user's input
	while (1) {

		// Part 1 
		// Print the user's environment variables
		printf("%s@%s:%s> ", getenv("USER"), getenv("MACHINE"), getenv("PWD") );

		char *input = get_input();
		//printf("whole input: %s\n", input);

		// Assume the input is valid
		bool validInput = true;

		tokenlist *tokens = get_tokens(input);


		for (int i = 0; i < tokens->size; i++) {
			// Part 2
			if(tokens->items[i][0] == '$') {
				environment_variable(tokens, i);
			}

			//Part 3
			if(tokens->items[i][0] == '~') {	
				tilde_expand(tokens, i);
			}

			// Part 4
			// We want to run path search on our first token always unless it's cd, exit, or jobs
			if ((i == 0) && ((strcmp(tokens->items[i], "cd") != 0) && (strcmp(tokens->items[i], "exit") != 0) && (strcmp(tokens->items[i], "jobs") != 0))) {
				// We want to replace the command in our tokenlist with the path with command
				char* cmdPath = pathSearch(tokens->items[i]);
				if (cmdPath != NULL) {
					// Free our tokens
					free(tokens->items[i]);
					tokens->items[i] = cmdPath;	
				}
				else {
					validInput = false;
				}
			}
				
		}

		// Part 9: Hangle cd, exit, and jobs commands
		if (strcmp(tokens->items[0], "cd") == 0) {
            CD(tokens);
		}
		else if (strcmp(tokens->items[0], "exit") == 0) {
            EXIT(tokens);
		}
		else if (strcmp(tokens->items[0], "jobs") == 0) {
            JOBS(tokens);
		}

		else if(tokens->items[(tokens->size) - 1][0] == '&')
			BackgroundProcess(tokens, input);
			
		// Part 7: Piping
		else if (checkPipe(tokens)) {
           	runPipe(tokens);
       	} 
		else {
           	// Part 5: Execute a single command if no pipes are found
           	executeCommand(tokens, input);
      	}
		
		// Add our input command to the command history if valid
		if (validInput) {
			strncpy(command_history[history_index % MAX_HISTORY], input, MAX_COMMAND_LENGTH);
    		command_history[history_index % MAX_HISTORY][MAX_COMMAND_LENGTH - 1] = '\0';
    		history_index++;
		}

		// After each input and execution, free our input and tokens
		free(input);
		free_tokens(tokens);
	
	}

	return 0;
}

//------------------------------------------------------------------------------------//

//Part 2

void environment_variable(tokenlist* TL, int i)
{
	//Create with enough space to hold the current unchanged token	
	char *newToke = (char*) malloc( (strlen(TL->items[i]) + 1) * sizeof(char));
	//copy the current unchanged token into the NewToke cstring
	strncpy(newToke, TL->items[i] + 1, strlen(TL->items[i]));
	//If the length of the environmental varible is larger than the
	//size of the unchanged token, reallocate enough space.
	if( (strlen(TL->items[i])) < (strlen(getenv(newToke))) )
	TL->items[i] = realloc(TL->items[i],  strlen(getenv(newToke)) + 1 );
	//Copy over the environmental variable into the Main array of tokens
	strcpy(TL->items[i], getenv(newToke));
	printf("In function: %s\n", TL->items[i]);
	//Free recently made pointer
	free(newToke);
}

//------------------------------------------------------------------------------------//

//Part 3

void tilde_expand(tokenlist* TL, int i)
{
	//Reallocate enough space for the original token to accept the expanded tilde
	TL->items[i] = realloc(TL->items[i], (strlen(getenv("HOME")) + strlen(TL->items[i]) + 1) );
			
	char* temp = malloc( strlen(getenv("HOME")) + 1);
 	strcpy(temp, getenv("HOME"));
	//Attach the other portion of the original token to the end of the expanded tilde
	if(TL->items[i][1] == '/')
	{
	temp = realloc(temp, (strlen(temp) + strlen(TL->items[i]) ) );
	strcpy(TL->items[i], strcat(temp, TL->items[i]+1));
	}
	//Copy the $HOME variable back into the original token
	else
	strcpy(TL->items[i], temp);
	//Free the temp pointer
 	free(temp);	
}

//------------------------------------------------------------------------------------//

// Part 4

// Function takes in the user's command and searches for the path
char* pathSearch(char* command) {
	// Get the user's path in one string
	char* PATHS = (char*) malloc(strlen(getenv("PATH")));
	strcpy(PATHS, getenv("PATH"));

	// Now iterate through that string stopping at each path, adding the search and checking access
	char* PATH = strtok(PATHS, ":");
	while(PATH != NULL) {
		// Create a new string which is the path + / + the user input
		char* file = malloc(strlen(PATH) + strlen(command) + 1);
		strcpy(file, PATH);
		strcat(file, "/");
		strcat(file, command);

		// Check if we can access this file, if we can this is what we return
		if (access(file, X_OK) == 0) {
			return file;
		}
		//printf("%s\n", command);
		// If we can't access this, go to the next
		PATH = strtok(NULL, ":");
	}

	// If no executable file is found, print an error message and return NULL
    printf("Path %s not found.\n", command);
	return NULL;
}

//------------------------------------------------------------------------------------//

// Part 5 & 6 

// Function to find redirection operators and files in the tokenlist
int findRedirection(tokenlist* TL, char* operator, char** filename) {
    for (int i = 0; i < TL->size; i++) {
        if (strcmp(TL->items[i], operator) == 0 && i + 1 < TL->size) {
            *filename = TL->items[i + 1];
            return i;
        }
    }
	// If no redirection needed, return -1
    return -1;
}

// Function to redirect IO if necessary
void redirectIO(char* inFile, char* outFile) {
    // Handle input redirection if needed
    if (inFile != NULL) {
        int fd_in = open(inFile, O_RDONLY);
        if (fd_in == -1) {
            perror("Failed to open input file");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_in, STDIN_FILENO) == -1) {
            perror("Failed to redirect standard input");
            exit(EXIT_FAILURE);
        }
        close(fd_in);
    }

    // Handle output redirection if needed
    if (outFile != NULL) {
        int fd_out = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd_out == -1) {
            perror("Failed to open output file");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd_out, STDOUT_FILENO) == -1) {
            perror("Failed to redirect standard output");
            exit(EXIT_FAILURE);
        }
        close(fd_out);
    }
}

// Function to remove the tokens as necessary for IO redirection
void removeTokens(tokenlist* TL, int start, int end) {
    int numToRemove = end - start + 1;
    for (int i = start; i <= TL->size - numToRemove; i++) {
        TL->items[i] = TL->items[i + numToRemove];
    }
    TL->size -= numToRemove;
}

// Function takes in the user's command, path searches, and executes the command 
// This function also handles I/O redirection using the redirectIO function
void executeCommand(tokenlist* TL, char* ori_command) {
	// Create our IO redirection files
	char* inFile = NULL;
	char* outFile = NULL;

	// Use our find_redirection function to get our redirection operators
	int inOperator = findRedirection(TL, "<", &inFile);
    int outOperator = findRedirection(TL, ">", &outFile);

	// Now, we must fork to create a child process
	pid_t pid = fork();

	// Check if our fork failed
	if (pid < 0) {
		perror("Fork failed");
		free(TL->items[0]);
		return;
	}
	// Check if we got a child process
	else if (pid == 0) {
		// Use our redirect IO function as needed
		redirectIO(inFile, outFile);

		// Remove redirection symbols and filenames from arguments if necessary
        if (inOperator != -1) {
            // Remove "< inFile"
            removeTokens(TL, inOperator, inOperator + 1);
        }
        if (outOperator != -1) {
            // Adjust out_index if input redirection was also removed
            outOperator = (inOperator != -1 && outOperator > inOperator) ? outOperator - 2 : outOperator;
            // Remove "> outFile"
            removeTokens(TL, outOperator, outOperator + 1);
        }

		// Execute our command on the child process
		execv(TL->items[0], TL->items);
	}
	// Check if we are still on the parent process
	else {
		int status;
		if(ori_command[strlen(ori_command)-1] == '&')
		{
		ori_command[strlen(ori_command)-1] = '\0';
		AddedJob(pid, ori_command);
		job_num++;
		}
		else{
		// Wait for the child process to finish
		waitpid(pid, &status, 0);
		}
	}
}

//------------------------------------------------------------------------------------//

// Part 7
//Checks if the piping command is being used
int checkPipe(tokenlist *tokens) {
    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            return 1; 
        }
    }
    return 0; 
}

int findNextPipeIndex(tokenlist *tokens, int startIndex) {
    for (int i = startIndex; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            return i;
        }
    }
    return tokens->size; // Return the size as the end indicator if no pipe is found.
}

char **buildCmdArgs(tokenlist *tokens, int start, int end) {
    int argc = end - start;
    char **argv = (char **)malloc((argc + 1) * sizeof(char *)); // +1 for NULL termination
    for (int i = 0; i < argc; i++) {
        argv[i] = tokens->items[start + i];
    }
    argv[argc] = NULL; 
    return argv;
}

//Counts the numbers of pipes needed. 
int countPipes(tokenlist *tokens) {
    int count = 0;

    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            count++;
        }
    }
    return count;
}

void runPipe(tokenlist *tokens) {

    int num_pipes = countPipes(tokens); // Implement this to count "|" in tokens

    int pipefds[2 * num_pipes];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("Failed to create pipes");
            exit(EXIT_FAILURE);
        }
    }

    int startIndex = 0;
    for (int i = 0; i <= num_pipes; i++) {
        int nextPipeIndex = findNextPipeIndex(tokens, startIndex);
        char **cmdArgs = buildCmdArgs(tokens, startIndex, nextPipeIndex);

        pid_t pid = fork();
        if (pid == 0) { // Child process
            // Setup redirection
            if (i > 0) { // If not the first command, get input from the previous pipe
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < num_pipes) { // If not the last command, output to the next pipe
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            // Close all pipes in the child
            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipefds[j]);
            }
            // Execute the command
            char *cmdPath = (cmdArgs[0][0] == '/') ? cmdArgs[0] : pathSearch(cmdArgs[0]);
            execv(cmdPath, cmdArgs);
            perror("Failed to execute command");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("Failed to fork");
            exit(EXIT_FAILURE);
        }
        startIndex = nextPipeIndex + 1; // Move startIndex past the current pipe
    }

    // Parent closes all pipes
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Parent waits for all child processes
    for (int i = 0; i <= num_pipes; i++) {
        wait(NULL);
    }
}

//------------------------------------------------------------------------------------//

// Part 8

void AddedJob(pid_t pid, char* command) {
    for (int i = 0; i < MAX_JOBS; i++) {
		// Check if the slot is available to add a job, if so we can add the job
        if (background_jobs[i] == 0) {
            background_jobs[i] = pid;
            strncpy(background_commands[i], command, MAX_COMMAND_LENGTH - 1);
            background_commands[i][MAX_COMMAND_LENGTH - 1] = '\0';
			active_jobs++;
            break;
        }
    }
}

void checkBackground()
{
int pid, status;

while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (int i = 0; i < 10; i++) {
        if (pids_jobs[i] == pid) {
            if (WIFEXITED(status)) {
                printf("[%d]    Done\t%s\n", i+1, commands[i]);
                pids_jobs[i] = 0; 
                break;
            }
        }
    }
	
}

}
void BackgroundProcess(tokenlist* TL, char* input)
{
	TL->items[TL->size - 1] = '\0';
	TL->size = TL->size -1;
	executeCommand(TL, input);
	checkBackground();
}


//------------------------------------------------------------------------------------//

// Part 9

// Handle the CD command
void CD(tokenlist* TL) {
    const char *PATH;

    // If only "cd", we want to change to the home directory
    if (TL->size == 1) {
        PATH = getenv("HOME");
        if (chdir(PATH) != 0) {
            perror("cd");
            return;
        }
    } 
    // If one argument is supplied, we want to go to the path after "cd"
    else if (TL->size == 2) {
        PATH = TL->items[1];
        if (chdir(PATH) != 0) {
            // Error handling
            perror("cd");
            return;
        }
    } 
    // If more than one argument, print an error
    else {
        fprintf(stderr, "Too many arguments for cd.\n");
        return;
    }

    // Update PWD environment variable to the new directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
		// Update PWD to the current working directory
        setenv("PWD", cwd, 1);
    } 
	else {
        perror("getcwd() error");
    }
}

// Handle the exit command
void EXIT(tokenlist* TL) {
	// Wait for any background processes to finish
	for (int i = 0; i < active_jobs; i++) {
        if (background_jobs[i] != 0) {
			// Wait for the background job to finish
            waitpid(background_jobs[i], NULL, 0);
        }
    }

	// Print the last 3 valid commands
	printf("Last (3) valid commands:\n");
    int start = history_index - MAX_HISTORY;
	if (start < 0) {
		start = 0;
	}
    for (int i = start; i < history_index; i++) {
        printf("[%d]: %s\n", i + 1, command_history[i % MAX_HISTORY]);
    }
	if (1) {
		// Exit the program
		exit(0);
	}
}

// Handle the jobs command
void JOBS(tokenlist* TL) {
    int activeJobs = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (background_jobs[i] != 0) {
            // Check if the job is still active
            if (kill(background_jobs[i], 0) == 0) {
				// If here, a process exists, so lets print it
                printf("[%d]+ %d %s\n", i + 1, background_jobs[i], background_commands[i]);
                activeJobs = 1;
            } 
			// If a job does not exist, lets clean up some memory
			else {
                background_jobs[i] = 0;
                memset(background_commands[i], 0, MAX_COMMAND_LENGTH);
            }
        }
    }
	// If we found no active jobs, print that
    if (activeJobs == 0) {
        printf("No active background processes.\n");
    }
}
//------------------------------------------------------------------------------------//

// Functions to get input and set up tokenization below
char *get_input(void) {
	char *buffer = NULL;
	int bufsize = 0;
	char line[5];
	while (fgets(line, 5, stdin) != NULL)
	{
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;
		buffer = (char *)realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;
		if (newln != NULL)
			break;
	}
	buffer = (char *)realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;
	return buffer;
}

tokenlist *new_tokenlist(void) {
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item) {
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);
	tokenlist *tokens = new_tokenlist();
	char *tok = strtok(buf, " ");
	while (tok != NULL)
	{
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}
	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens) {
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
