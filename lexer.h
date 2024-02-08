#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    char ** items;
    size_t size;
} tokenlist;

int job_num;
int pids_jobs[10];
char commands[10][100];
void checkBackground();
void BackgroundProcess(tokenlist* TL, char* input);
void AddedJob(int pid, char* command);
void environment_variable(tokenlist* TL, int i);
void tilde_expand(tokenlist* TL, int i);
char* pathSearch(char* command);
int findRedirection(tokenlist* TL, char* operator, char** filename);
void redirectIO(char* inFile, char* outFile);
void removeTokens(tokenlist* tokens, int start, int end);
void executeCommand(tokenlist* TL, char* ori_command);
void CD(tokenlist* TL);
void EXIT(tokenlist* TL);
void JOBS(tokenlist* TL);
char* get_input(void);
tokenlist* get_tokens(char *input);
tokenlist* new_tokenlist(void);
void add_token(tokenlist* tokens, char* item);
void free_tokens(tokenlist* tokens);
int checkPipe(tokenlist* tokens);
void runPipe(tokenlist* tokens);
int countPipes(tokenlist* tokens);
int findNextPipeIndex(tokenlist *tokens, int startIndex);
char **buildCmdArgs(tokenlist *tokens, int start, int end);

