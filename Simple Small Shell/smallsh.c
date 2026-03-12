//signal.h was not registering struct sigaction as valid syntax
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>


typedef struct pidInfo pidInfo;

struct pidInfo{
	pid_t pidNum;
	pidInfo* next;
	pidInfo* prev;
};

volatile sig_atomic_t foreG_mode =1;

//I think it might be necssary to create a handler which changes 
//this for every function call(except built-in)
volatile sig_atomic_t signal_status;
volatile sig_atomic_t exit_status = 0;


//On sigstop (CTRL-Z) call the below handler
void foreGroundChange(int signal){
	if(foreG_mode){
		write(STDOUT_FILENO, "\nEntering foreground only mode\n",31);
		foreG_mode = 0;
	}else{
		write(STDOUT_FILENO, "\nExiting foreground only mode\n",30);
		foreG_mode = 1;
	}
}

pidInfo* addProces(pidInfo* pid, pid_t pidNum, int foreground ){
	pidInfo* current = pid;
	while(current->next != NULL){
		current = current->next;
	}
	// reached last process in list
	pidInfo* newProcess = (pidInfo*) malloc(sizeof(pidInfo));
	current->next = newProcess;
	newProcess->prev = current;
	newProcess->next = NULL;
	newProcess->pidNum = pidNum;
}

pidInfo* deleteProcess(pidInfo* head, pid_t processNum){
	pidInfo* current = head;
	do{
		current = current->next;
		if(current->pidNum == processNum){
			current->prev->next = current->next;
			if(current->next != NULL){
				current->next->prev = current->prev;
			}
			free(current);
			return head;
		}
	}while(current->next !=NULL);
	write(STDOUT_FILENO, "\n couldn't find process to delete,",35);
return NULL;
}

void exit_program(pidInfo* pid){
	int exit_stat = 0;

	if(pid->next != NULL){
		pid = pid->next;
		do{
			kill(pid->pidNum,SIGKILL);
			waitpid(pid->pidNum,&exit_stat,0);
			printf("killed child process - %d with exit status %d\n",pid->pidNum,exit_stat);
			pidInfo *temp = pid;
			pid = pid->next;
		
			free(temp);

		}while(pid != NULL);
}
	write(STDOUT_FILENO, "\n Exiting the program \n",23);
	exit(0);
}

//Have to rewrite this to print which ever is the last changed?
void status(){
	printf("%d\n", exit_status);
}


//will need to store currently running child process and possibly terminated ones
//will need to use the stored processes to check for terminated children to wait on 
//using a while loop
int main() {
	pidInfo *pidHead = (pidInfo*) malloc(sizeof(pidInfo));
	pidHead->next = NULL;
	pidHead->prev = NULL;

	struct sigaction initSig = {0};
	//main process CTRL-C handling
	initSig.sa_handler = SIG_IGN;
	sigaction(SIGINT,&initSig,NULL);

	//main process CTRL-Z handling, should remain the same for children
	initSig.sa_handler = foreGroundChange;
	sigemptyset(&initSig.sa_mask);
	initSig.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &initSig,NULL);
	while(1){
		int pidExitStat;
		pid_t result;
		if(pidHead->next !=NULL){
			pidInfo *temp = pidHead->next;
			while(temp!=NULL){
				pidInfo *nextNode = temp->next;

				result = waitpid(temp->pidNum,&pidExitStat,WNOHANG);
				//If error occured with process
				if(result == -1){
					perror("Something wrong with waiting on current process:");
					temp = nextNode;
					//If process terminated
				}else if(result  == temp->pidNum){
					if(WIFEXITED(pidExitStat)){
						printf("background pid %d: exit value %d\n", 
                        temp->pidNum, WEXITSTATUS(pidExitStat));
					}else if(WIFSIGNALED(pidExitStat)){
                		printf("background pid %d: terminated by signal %d\n",
                        temp->pidNum, WTERMSIG(pidExitStat));
            }
					pidHead = deleteProcess(pidHead,temp->pidNum);
					temp = nextNode;
				//else if process is still alive
				}else{
					temp = nextNode;
				}
			}
	}
		//Print user prompt even if process is running
		printf(":");
		fflush(stdout);
		char *tempTextLine = NULL;
    	size_t buffer = 0;
    	size_t line_length = getline(&tempTextLine, &buffer, stdin);
		//if comment/empty line re-prompt user
		if(tempTextLine[0]=='#' || tempTextLine[0]=='\n'){
			free(tempTextLine);
			continue;
		}
		//scrape command line for $$ if exist replace with process ID
		char* textLine = malloc(4096);
		char processIDstr[20];
		sprintf(processIDstr, "%d", getpid());

		char* readPos = tempTextLine;
		char* writePos = textLine;

		while(*readPos != '\0'){
			if(readPos[0] == '$' && readPos[1] == '$'){
				// found $$ copy in PID string
				strcpy(writePos, processIDstr);
				writePos += strlen(processIDstr);
				// dont reread the $$
				readPos += 2;  
			}else{
				// increment both but also set the current readPos char
				//the current writePos char (incremenet the pointer and deref it to access char)
				*writePos++ = *readPos++;
			}
		}
		*writePos = '\0';

		// Remove newline
		int len = strlen(textLine);
		if(len > 0 && textLine[len-1] == '\n'){
			textLine[len-1] = '\0';
		}

		free(tempTextLine);
		//get process name
		char* command = strtok(textLine," ");
		//If command is built in, use below 3 if statements
		if(strcmp(command,"status") == 0){
			status();
			continue;
		}

		if(strcmp(command,"exit") == 0){
			free(textLine);
			exit_program(pidHead);
		}

		if(strcmp(command,"cd") == 0){
			//Store path argument(if it exists)
			char* location = strtok(NULL, "\0");
			//check if path argument was provided, if not take me home!
			if(location == NULL){
				location = getenv("HOME");
			}
			//check if the change of working directory failed, if so print error			
			if(chdir(location) != 0){
				perror("cd failed");
			}

		free(textLine);
		continue;
		}
		//IF tihs point is reached, command is not built in
		//so we need to store arguments, input and output files, and fork process
		char* args[20];
		char* inFile = NULL;
		char* outFile = NULL;
		int background = 0;
		int i = 0;

		//command will be first agument in the array when we call the function
		args[i++] = command;

		// parse arguments
		char* token;
		while((token = strtok(NULL, " ")) != NULL){
			// check for input file redirection
			if(strcmp(token, "<") == 0){
				token = strtok(NULL, " ");
				if(token != NULL){
					inFile = token;
				}
			}
			// check for output file redirection
			else if(strcmp(token, ">") == 0){
				token = strtok(NULL, " ");
				if(token != NULL){
					outFile = token;
				}
			}
			// check if its a background process
			else if(strcmp(token, "&") == 0){
				background = 1;
			}
			//If regular argument, add to args array
			else{
				args[i++] = token;
			}
		}
		//last argument must be NULL when we call the function
		args[i] = NULL;		
		


			pid_t fork_result = fork();
			if(fork_result == 0){
				//if command is a background command, redirect to /dev/null
				if(background == 1){
					if(inFile == NULL){
						int dnull = open("/dev/null",O_RDONLY);
						dup2(dnull, STDIN_FILENO);
					}
					// for both input and output file
					if(outFile==NULL){
						int dnull= open("/dev/null",O_WRONLY);
						dup2(dnull, STDOUT_FILENO);
					}
				}
				//Check if command line included input file
				//If so, open and redirect
				if(inFile != NULL){
					int input_file = open(inFile, O_RDONLY,0);
					if(input_file==-1){
						perror("cannot open input file");
					}
					dup2(input_file, STDIN_FILENO);
				}
				//Check if command line included output file
				//If so, open and redirect
				if(outFile != NULL){
					int output_file = open(outFile,O_WRONLY | O_TRUNC | O_CREAT,0644);
					if(output_file==-1){
						perror("cannot open output file");
					}
					dup2(output_file, STDOUT_FILENO);
				}
				//All child proceses must ignore sigtstop (CTRL-Z)
				initSig.sa_handler = SIG_IGN;
				sigaction(SIGTSTP,&initSig,0);
				
				if(background == 1 && foreG_mode == 1){
					//background process, SIGINT is still ignored, inhereted from parent
					//Checks if command runs correctly, if not, just return to user prompt
					if(execvp(command,args)==-1){
						perror(command);
						exit(1);
					}
					
				}else{ 
					//foreground process,SIGINT NOW KILLS 
					initSig.sa_handler = SIG_DFL;
					sigaction(SIGINT,&initSig,NULL);

					if(execvp(command, args)==-1){
						perror("could not run that program:");
						exit(1);
					}
				}
			}else{
				//parent process

				//if the child process ran in background and not in foreground only mode 
				//wait with WNOHANG and add to process linked list
				int childExit = exit_status;
				if(background == 1 && foreG_mode == 1){
					printf("background pid is %d\n", fork_result);
					waitpid(fork_result,&childExit,WNOHANG);
					addProces(pidHead,fork_result,0);
				}else{
					//Otherwise, run waitpid without WNHOHANG and wait for and print exit status
					waitpid(fork_result, &childExit,0);
					if(WIFEXITED(childExit)!=0){
						exit_status = WEXITSTATUS(childExit);
					}else if(WIFSIGNALED(childExit)!=0){
						exit_status= WTERMSIG(childExit);
						printf("terminated by signal %d\n", exit_status);

					}
				}				
				}
				//we cant forget about freeing the textLine :)
				free(textLine);
			}
		
		}
	




