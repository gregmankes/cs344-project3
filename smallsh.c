/******************************************************************************
 * smallsh.c
 * 
 * Author: Gregory Mankes
 * Description: A small shell with three built in commands: exit, cd, and
 * status. All other commands will be forked and executed using the system
 * environment path.
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

// Function declarators
void exit_shell(int*);
void get_status(int*);
void handle_fork_exec(int*, int, struct sigaction, char *, char *, char **);
void prompt(char *);
void change_directory(char **);
char ** create_char_array(int, int);
void delete_char_array(char **, int);
void run_shell();
void wait_for_children(int*);
char ** reset_command_array(char **, int, int);

/******************************************************************************
 * void wait_for_children(int *)
 * 
 * Waits for any child process that hasn't completed yet
 *****************************************************************************/
void wait_for_children(int *status){
	pid_t pid = waitpid(-1, status, WNOHANG);
	while(pid > 0){
		printf("Background process %d closed\n", pid);
		get_status(status);
		pid = waitpid(-1, status, WNOHANG);
	}
}

/******************************************************************************
 * void handle_fork_exec(int*, int, struct sigaction, char *, char *, char **)
 * 
 * Handles the fork and then the execution from there on. Will have the parent
 * wait for a child process if the child is in the foreground. Will not wait
 * if child is in the background.
 *****************************************************************************/
void handle_fork_exec(int * status, int fg, struct sigaction act, char * output_filename, char * input_filename, char ** commands){
	// fork the parent and the child processes
	pid_t pid = fork();
	// enter child/parent/error differentiator
	if (pid == 0){
		// this is the child
		// intialize file desciptor, redirect number, filename pointer, and execution result number
		int fd;
		int redirect;
		char * to_open = NULL;
		int exec_result;
		if(fg){
			// if we are in the foreground, we want to be able to be interrupted
			act.sa_handler = SIG_DFL;
			act.sa_flags = 0;
			sigaction(SIGINT, &act, NULL);
			// if there is an input file, set the filename pointer to be the input filename
			if(strcmp(input_filename, "") != 0)
				to_open = input_filename;
		}
		else{ // we are in the background
			// set the input filename pointer to be /dev/null
			to_open = "/dev/null";
		}
		if (to_open != NULL){
			// we either have an input file or we are in the background
			// either way open a file descriptor to open the input file
			fd = open(to_open, O_RDONLY);
			// check for errors
			if(fd < 0){
				fprintf(stderr, "Error opening input file\n");
				fflush(stdout);
				exit(1);
			}
			// try to redirect the input to the file
			redirect = dup2(fd, 0);
			// if it fails exit
			if(redirect < 0){
				fprintf(stderr, "Error redirecting the input\n");
				fflush(stdout);
				exit(1);
			}
			close(fd);
		}
		if(strcmp(output_filename, "") != 0){
			// in this case, we have an output file
			// open the output file with writeonly & and create it and truncate all
			// input to the end
			fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			// check for errors
			if(fd < 0){
				fprintf(stderr, "Error opening output file\n");
				fflush(stdout);
				exit(1);
			}
			// try to redirect the output to the file
			redirect = dup2(fd, 1);
			// if it fails exit
			if(redirect < 0){
				fprintf(stderr, "Error redirecting the output\n");
				fflush(stdout);
				exit(1);
			}
			close(fd);
		}
		// attemp to exec (since this only happens when we are not using a built in command)
		exec_result = execvp(commands[0], commands);
		// if the exec result is not 0, we had an error, print that
		if(exec_result){
			fprintf(stderr,"smallsh did not recognize the command: %s\n", commands[0]);
			fflush(stdout);
			exit(1);
		}
		
	}
	else if (pid < 0){// if the pid is negative
		// whoops, forking error
		// exit shell
		fprintf(stderr, "error in fork\n");
		*status = 1;
		exit_shell(status);
	}
	else{
		// we are the parent
		// if we are in the bg, just wait until child is done
		if(fg){
			waitpid(pid, status, 0);
			get_status(status);
		}
		else{
			// print the background process id
			printf("Background process id number %d\n", pid);
		}
	}

}

/******************************************************************************
 * void exit_shell(int*)
 * 
 * Exits the shell. Cleans up unfinished processes first.
 *****************************************************************************/
void exit_shell(int * status){
	// wait for unfinished children
	wait_for_children(status);
	// exit shell
	exit(*status);
}

/******************************************************************************
 * void prompt(char *)
 * 
 * Takes user input and places it into the char pointer input
 *****************************************************************************/
void prompt(char * input){
	// print the prompt
	printf(": ");
	// flush the output stream
	fflush(stdout);
	// get the input from the user
	if(fgets(input, 2049, stdin) == NULL){
		exit(0); // in the case that we reached the end of an input file
	}

}

/******************************************************************************
 * void get_status(int*)
 * 
 * gets the status of the shell using the status pointer, the WIFEXITED and 
 * WEXITSTATUS macros. If the process exited normally, we print the exit 
 * status. Else, the process was killed by a signal and we print the signal.
 *****************************************************************************/
void get_status(int *status){
	// if the status indicates that the process exited
	if(WIFEXITED(*status)){
		// let the user know that this is the case
		printf("The process exited normally\n");
		int exitstatus = WEXITSTATUS(*status);
		printf("The exit status was %d\n", exitstatus);
	}
	else{// the process was killed by a signal
		printf("The process was terminated by a signal %d\n", *status);
	}
	
}

/******************************************************************************
 * void change_directory(char **)
 * 
 * Changes the directory to the directory specified by the commands string 
 * array. If there is no directory specified, change to home directory.
 *****************************************************************************/
void change_directory(char ** commands){
	// if the user specified a directory
	if (commands[1] != NULL){
		// change to the directory
		chdir(commands[1]);
	}
	else{// change to home
		chdir(getenv("HOME"));
	}
}

/******************************************************************************
 * char ** create_char_array(int, int)
 * 
 * creates an array of strings to be used for the commands. allocates on the
 * heap.
 *****************************************************************************/
char ** create_char_array(int arraysize, int stringsize){
	// create the array of pointers to strings
	char ** to_return = malloc(arraysize* sizeof(char *));
	int i = 0;
	for(; i < arraysize; i++){
		// allocate the strings themselves
		to_return[i] = malloc(stringsize*sizeof(char));
	}
	// return the array of strings pointer
	return to_return;
}

/******************************************************************************
 * void delete_char_array(char **, int)
 * 
 * deletes the memory allocated to the array of strings
 *****************************************************************************/
void delete_char_array(char ** to_delete, int arraysize){
	int i = 0;
	for (; i < arraysize; i++){
		// delete the strings
		free(to_delete[i]);
	}
	// delete the array of pointers to strings
	free(to_delete);
}

/******************************************************************************
 * char ** reset_command_array(char**, int, int)
 * 
 * deletes the char array, and reallocates it. Also, memsets all of the
 * strings to be 0. This is to avoid a weird strcpy offset error, where
 * commands[1] is set to NULL and thus is no longer a char *.
 *****************************************************************************/
char ** reset_command_array(char ** commands, int arraysize, int stringsize){
	// delete the array of strings
    delete_char_array(commands, arraysize);
	// create a new array of strings
	char ** new_array = create_char_array(arraysize, stringsize);
	// memset every string to be 0's
	int i = 0;
	for (;i < arraysize; i++){
		memset(new_array[0], 0, stringsize);
	}
	// return the new array of strings
	return new_array;
}

/******************************************************************************
 * void run_shell
 * 
 * runs the shell, calling all above functions.
 *****************************************************************************/
void run_shell(){
	//setup to ignore a signal interrupt
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigfillset(&(act.sa_mask));
	sigaction(SIGINT, &act, NULL);

	// create an input buffer
	char * input = malloc(2049 * sizeof(char));
	// create a command string
	char ** commands = create_char_array(513, 513);
	// create indicators for foreground and background
	int fg = 0;
	// create input file and output file buffers
	char * input_filename = malloc(100 * sizeof(char));
	char * output_filename = malloc(100 * sizeof(char));
	// create a buffer for the tokens in the string tokenizer
	char * tok;
	// keep track of the status
	int status = 0;
	// set all of the strings to be all 0's (make sure that they are clean)
	memset(input, 0, sizeof(input));
	memset(input_filename,0, sizeof(input_filename));
	memset(output_filename, 0, sizeof(output_filename));
	// run forever until we type exit
	while(1){
		// initialize shell to be in the foreground
		fg = 1;
		// prompt the user for input
		prompt(input);
		
		// determine what our input is
		int i = 0;
		// get our first token
		tok = strtok(input, " \n");
		// while we have more tokens
		while(tok != NULL){
			// if we are redirecting output
			if(strcmp(tok, ">") == 0){
				// grab the next token because it
				// will be the name of the output file
				tok = strtok(NULL, " \n");
				// copy that token to the output filename
				strcpy(output_filename, tok);
				// get the next token (most likely null)
				tok = strtok(NULL, " \n");
			}
			else if(strcmp(tok, "<") == 0){
				// in this case we are injecting input
				// grab the next token to grab the name
				// of the input file
				tok = strtok(NULL, " \n");
				// copy the name of the input file to the input filename
				strcpy(input_filename, tok);
				// grab the next token
				tok = strtok(NULL, " \n");
			}
			else if(strcmp(tok, "&") == 0){
				// in this case, we want the process to run
				// in the background, set fg to 0 and break
				// since this is the last arg in the command
				fg = 0;
				break;
			}
			else{// we are not using a special character
				// copy the contents of the string to the
				// command array and grab the next token
				strcpy(commands[i], tok);
				tok = strtok(NULL, " \n");
				i++;
			}
		}
		// must set the last arg to null, since we will use it in
		// the exec args
		if (i != 0)
			commands[i] = NULL;

		// if the first character of the first command is a comment, then skip it
		if(strcmp(commands[0], "") == 0 || strcmp(commands[0], "\n") == 0){
		    // don't do anything
		}
		else if(*(commands[0]) == '#'){
			// don't do anything
		}
		else if (strcmp(commands[0], "cd") == 0){
			// We need to change the directory, call this function
			change_directory(commands);
		}
		else if (strcmp(commands[0], "status") == 0){
			// if the user wants the status, give it to them
			get_status(&status);
		}
		else if (strcmp(commands[0], "exit") == 0){
			// if the user wants to exit, exit
			// make sure that we don't leak memory
			free(input);
			free(input_filename);
			free(output_filename);
			delete_char_array(commands, sizeof(commands));
			exit_shell(&status);
		}
		else{
			// the user passed in a command that is not built in, handle it.
			handle_fork_exec(&status, fg, act, output_filename, input_filename, commands);
		}
		// wait for any children that haven't finished processing
		wait_for_children(&status);
		// reset the input to empty
		memset(input, 0, sizeof(input));
		memset(input_filename,0, sizeof(input_filename));
		memset(output_filename, 0, sizeof(output_filename));
		commands = reset_command_array(commands, 513, 513);
	}

	// make sure that we don't leak memory
	free(input);
	free(input_filename);
	free(output_filename);
	delete_char_array(commands, sizeof(commands));
	// if we somehow get here, exit
	exit_shell(&status);
}

/******************************************************************************
 * int main()
 * 
 * main method. runs the shell.
 *****************************************************************************/
int main(){
	run_shell();
	return 0;
}
