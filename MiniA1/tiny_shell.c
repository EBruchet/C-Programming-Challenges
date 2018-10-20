#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define STACK_SIZE (1024 * 1024)

/*gcc -o tiny_shell tiny_shell.c*/

static char* get_a_line();
static int length(char* line);
static int my_system(char* line);
static double gettime();
int clone_function(void *arg);

// pipe_setting is set in the main from the third command line argument, and can be 0 or 1
// If 0, the tiny shell will read from the FIFO
// If 1, the tiny shell will write to the FIFO
int pipe_setting;

// pipe_path determines the name/path of the pipe used in the #PIPE implementation
char *pipe_path;


int main(int argc, char *argv[]){
	
	// The below if block is used only in the #PIPE implementation of my_system
	// If the user wants their shell to WRITE to the FIFO pipe, then argv[2] = 1
	// If the user wants their shell to READ from the FIFO pipe, then argv[2] = 0
	if(argc > 1){
		if(strcmp("1", argv[2]) == 0)
			pipe_setting = 1;
		else if(strcmp("0", argv[2]) == 0)
			pipe_setting = 0;
		else{
			printf("Invalid PIPE argument.");
			_exit(1);	
		}
		// The pipe_path is simply the NAME of the pipe in the same directory
		// as this file
		pipe_path = argv[1];
	}

	char *line = NULL;

	// Infinite loop that asks for user input (shell command) upon program execution
	// and following successful execution of previous command
	while(1){
		line = get_a_line();
		if (length(line) > 1){
			double start_time = gettime();
			my_system(line);
			double end_time = gettime();
			printf("Execution Time: %fms\n", end_time - start_time);
		}
	}
}


// Returns the user's input as an array of char's
char* get_a_line(){
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	read = getline(&line, &len, stdin);

	// If the user inputs the command 'exit', we want to kill the parent process,
	// which in this case is our tiny shell
	if(strcmp("exit\n", line) == 0)
		exit(0);
	else if(strcmp("\n", line) == 0)
		exit(0);
	return line;
}

// Returns the current system time as a double in microseconds
double gettime(){
	struct timespec ts;

	if(clock_gettime(CLOCK_REALTIME, &ts) < 0)
		perror("Clock-gettime");

	return ts.tv_sec * 1000000 + ts.tv_nsec/1000;
}


// Length makes use of the pre-existing strlen() function to compute the length
// of an array of characters
int length(char* line){
	return strlen(line);
}


// The function called by the clone() call
// Inside of this function we execute the standard contents of the child process
// as in the other implementations
int clone_function(void *arg){
	char *path = "/bin/sh";
	execl(path, path, "-c", arg, NULL);
	_exit(0);
}

// Custom-implementation of the system() call, flags provided upon compilation
// are used to determine which specific implementation is used
int my_system(char* line){
	#ifdef FORK
		pid_t pid;
		int fork_return;	
		char *path = "/bin/sh";
		
		pid = fork();
		if(pid < 0){
			// Fork() call failed
			fork_return = -1;
		} else if(pid == 0){
			// Inside the child process
			fork_return = 0;
			// System call used to execute the shell command
			execl(path, path, "-c", line, NULL);			
			_exit(0);

		} else if(pid > 0){
			// Parent Case
			fork_return = pid;
			// Force the parent to wait on the child process
			waitpid(pid, &fork_return, 0);
		}	

		return fork_return;

	#elif VFORK
		pid_t pid;
		int fork_return;
		char *path = "/bin/sh";	

		pid = vfork();
		if(pid < 0){
			fork_return = -1;	
		} else if(pid == 0){
			// Inside the child process
			fork_return = 0;
			// System call used to execute the shell command
			execl(path, path, "-c", line, NULL);
			_exit(0);
		} else if(pid > 0){
			// Parent Case
			// No action necessary in the parent as vfork will implicitly 
			// force the parent to wait on the child process to execute
			//fork_return = pid;
			//if(waitpid(pid, &fork_return, 0) == 0){
			//	fork_return = -1;
			//}
		}

		return fork_return;
	#elif CLONE
		char *stack;
		char *stack_top;
		pid_t child_pid;
		
		// Create the pseudo 'stack' by allocating memory for it
		stack = malloc(STACK_SIZE);
	
		// If null, there was an error in allocating memory for the stack
		if(stack == NULL){
			printf("Error allocating memory with malloc");
		}
						
		stack_top = stack + STACK_SIZE;

		// Set the appropriate clone call flags as seen in the tutorial slides
		int clone_flags = CLONE_VFORK | CLONE_FS;

		child_pid = clone(clone_function, stack_top, clone_flags | SIGCHLD, line);

		if (waitpid(child_pid, NULL, 0) == -1)
			perror("waitpid");
	#elif PIPE
		int fd;
		pid_t pid;
		char *path = "/bin/sh";
		mkfifo(pipe_path, 0666);
	
		pid = fork();
		if(pid < 0){
			// Fork failed
			perror("Fork call failed.");
		} else if(pid == 0){
			if(pipe_setting == 0){
				// This is the READING end of the pipe
				// Close the second entry of this processes' FD table, which is STD_IN
				// So input comes from the pipe
				close(0);
				open(pipe_path, O_RDONLY);
				execl(path, path, "-c", line, NULL);
				close(0);
				_exit(0);
			} else if(pipe_setting == 1){
				// This is the WRITING end of the pipe
				// Close the first entry in this processes' FD table, which is STD_OUT
				// so we route the output of our exec to the pipe when calling open()
				close(1);
				open(pipe_path, O_WRONLY);
				execl(path, path, "-c", line, NULL);
				close(1);
				_exit(0);
			} else{
				// The user did not have 0 or 1 in the command line arguments
				printf("Invalid PIPE my_system arguments.");
				_exit(1);
			}
		}
		
	
	#else
		//TODO:: If no compiler flag was mentioned
	#endif
}
