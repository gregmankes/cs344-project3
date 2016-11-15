#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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

void wait_for_children(int *status){
	pid_t pid = waitpid(-1, status, WNOHANG);
	while(pid > 0){
		printf("Background process %d closed\n", pid);
		get_status(status);
		pid = waitpid(-1, status, WNOHANG);
	}
}

void handle_fork_exec(int * status, int fg, struct sigaction act, char * output_filename, char * input_filename, char ** commands){
	pid_t pid = fork();

	if (pid == 0){
		// this is the child
		int fd;
		int redirect;
		char * to_open = NULL;
		int exec_result;
		if(fg){
			// if we are in the foreground, we want to be able to be interrupted
			act.sa_handler = SIG_DFL;
			act.sa_flags = 0;
			sigaction(SIGINT, &act, NULL);
			if(strcmp(input_filename, "") != 0)
				to_open = input_filename;
		}
		else{ // we are in the background
			to_open = "/dev/null";
		}
		if (to_open != NULL){
			fd = open(to_open, O_RDONLY);
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
			fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0744);
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
		exec_result = execvp(commands[0], commands);
		if(exec_result){
			fprintf(stderr,"smallsh did not recognize the command: %s\n", commands[0]);
			fflush(stdout);
			exit(1);
		}
		
	}
	else if (pid < 0){
		// whoops, forking error
		fprintf(stderr, "error in fork\n");
		*status = 1;
		exit_shell(status);
	}
	else{
		// we are the parent
		// if we are in the bg, just wait until child is done
		if(fg){
			waitpid(pid, status, 0);
		}
		else{
			// print the background process id
			printf("background process id number %d\n", pid);
		}
	}

}

void exit_shell(int * status){
	wait_for_children(status);
	exit(*status);
}

void prompt(char * input){
	printf(": ");
	fflush(stdout);
	if(fgets(input, 2049, stdin) == NULL){
		exit(0); // in the case that we reached the end of an input file
	}

}

void get_status(int *status){
	if(WIFEXITED(*status)){
		printf("The process exited normally\n");
		int exitstatus = WEXITSTATUS(*status);
		printf("The exit status was %d\n", exitstatus);
	}
	else{
		printf("The process was terminated by a signal %d\n", *status);
	}
	
}

void change_directory(char ** commands){
	if (commands[1] != NULL){
		chdir(commands[1]);
	}
	else{
		chdir(getenv("HOME"));
	}
}

char ** create_char_array(int arraysize, int stringsize){
	char ** to_return = malloc(arraysize* sizeof(char *));
	int i = 0;
	for(; i < arraysize; i++){
		to_return[i] = malloc(stringsize*sizeof(char));
	}
	return to_return;
}

void delete_char_array(char ** to_delete, int arraysize){
	int i = 0;
	for (; i < arraysize; i++){
		free(to_delete[i]);
	}
	free(to_delete);
}

char ** reset_command_array(char ** commands, int arraysize, int stringsize){
    delete_char_array(commands, arraysize);
	char ** new_array = create_char_array(arraysize, stringsize);
	int i = 0;
	for (;i < arraysize; i++){
		memset(new_array[0], 0, stringsize);
	}
	return new_array;
}

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
	memset(input, 0, sizeof(input));
	memset(input_filename,0, sizeof(input_filename));
	memset(output_filename, 0, sizeof(output_filename));
	
	while(1){
		fg = 1;
		prompt(input);
		
		// determine what our input is
		int i = 0;
		tok = strtok(input, " \n");
		while(tok != NULL){
			if(strcmp(tok, ">") == 0){
				tok = strtok(NULL, " \n");
				strcpy(output_filename, tok);
				tok = strtok(NULL, " \n");
			}
			else if(strcmp(tok, "<") == 0){
				tok = strtok(NULL, " \n");
				strcpy(input_filename, tok);
				tok = strtok(NULL, " \n");
			}
			else if(strcmp(tok, "&") == 0){
				fg = 0;
				break;
			}
			else{
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

int main(){
	run_shell();
	return 0;
}
