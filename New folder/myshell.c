#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <err.h>
#include <math.h>

/* gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 -o shell shell.c myshell.c */

/* zombie processes prevention : https://www.geeksforgeeks.org/zombie-processes-prevention/ */

/* signal handler */
void handler(int signum, siginfo_t *info, void *ptr) {}


/* Background child processes should not terminate upon SIGINT */
void background_sig_pro(){

	struct sigaction act = {
	  .sa_handler = SIG_IGN, 
	  .sa_flags = SA_RESTART
  	};  
  	
  	if (0 != sigaction(SIGINT, &act, NULL)) {
  		fprintf(stderr, "%s\n", strerror(errno));
    	exit(1);
    }
}

/* After prepare() finishes, the parent (shell) should not terminate upon SIGINT */
int prepare(void){

	struct sigaction act = {
	  .sa_handler = SIG_IGN, 
	  .sa_sigaction = handler,
	  .sa_flags = SA_RESTART
  	};  
  
  	if (0 != sigaction(SIGINT, &act, NULL)) {
  		fprintf(stderr, "%s\n", strerror(errno));
    	exit(1);
    }
    return 0;
    
}
/* 
* arglist - a list of char* arguments (words) provided by the user
* it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
* RETURNS - 1 if should continue, 0 otherwise 
*/
int process_arglist(int count, char **arglist){
	
	int pipe_ind = 0;
	
	/* pipe symbol '|' search */
	for(int i=1; i<count-1; i++){
		if (!strcmp(arglist[i],"|")){
			pipe_ind = i;
			break;
		}
	}
	
	/* Single piping
	* runing two child processes concurrently, with the standard output of the first process (executing the command that 
	* appears before the pipe symbol) piped to the input (stdin) of the second process (executing the 
	* command that appears after the pipe symbol). 
	* The shell waits until both commands complete before accepting another command.
	*/
	if (pipe_ind != 0){    
	
		arglist[pipe_ind]=NULL; /* getting the first command (before the pipe symbol) */
		int pfds[2];
    	if (pipe(pfds) == -1) { /* piping */
    		fprintf(stderr, "%s\n", strerror(errno));
    		exit(1);
    	}
    	
   		pid_t pid = fork();
   		
    	if (pid == -1) {
    		fprintf(stderr, "%s\n", strerror(errno));
    		exit(1);
    	}
    	else if (pid == 0){ /* child */
    		
    		if (dup2(pfds[1], 1) == -1) { /* piping the standard output of the first process */
    			fprintf(stderr, "%s\n", strerror(errno));
    			exit(1);
    		}
        	close(pfds[0]);
        	close(pfds[1]);
        	if (execvp(arglist[0], arglist) == -1) { /* execute first command */
    			fprintf(stderr, "%s: %s\n", arglist[0], strerror(errno));
    			exit(1);
    		}    	
    	}
    	else{ /* parent */
    		signal(SIGCHLD, SIG_IGN); /* zombie processes prevention */
    		
        	pid_t pid2 = fork();
        	
       		if (pid2 == -1) {
    			fprintf(stderr, "%s\n", strerror(errno));
    			exit(1);
    		}
        	else if (pid2 == 0){ /* child */
            	if (dup2(pfds[0], 0) == -1) { /* piping the standard input of the second process */
    				fprintf(stderr, "%s\n", strerror(errno));
    				exit(1);
    			}
            	close(pfds[0]);
            	close(pfds[1]); 
            	if (execvp(arglist[pipe_ind + 1], arglist + (pipe_ind + 1)) == -1) { /* execute second command (after the pipe symbol)*/
            		fprintf(stderr, "%s: %s\n", arglist[pipe_ind + 1], strerror(errno));
    				exit(1);
    			}
        	}
        	else{ /* parent (shell) */
        		signal(SIGCHLD, SIG_IGN); /* zombie processes prevention */
            	close(pfds[0]);
            	close(pfds[1]); 
            	/* waiting for both processes to complete */
            	if(waitpid(pid, NULL, WUNTRACED)==-1 && errno!=EINTR && errno!=ECHILD){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1); 
				}	
				if(waitpid(pid2, NULL, WUNTRACED)==-1 && errno!=EINTR && errno!=ECHILD){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1); 
				}	
            	return 1;
        	}
    	}
	}
	/* Output redirecting 
	* creat/open the specified file (that appears after the redirection symbol) and then run the child process, 
	* (the command that appears before the redirection symbol) with the standard output redirected to the output file.
	* The shell waits for the command to complete before accepting another command.
	*/
	else if (count >1 && !strcmp(arglist[count -2],">")){ 
	
  		int fd = open(arglist[count -1], O_RDWR | O_CREAT, 0644); /* overwriting/creating output file */
  		
  		if (-1 == fd) {
    		fprintf(stderr,"%s\n", strerror(errno));
    		exit(1);
  		}
  		arglist[count-2]=NULL;	/* getting the command (before the redirection symbol) */
		pid_t pid =fork();
		if (pid==-1){ 
			fprintf(stderr, "%s\n", strerror(errno));
			close(fd);
			exit(1);
		}
		else if (pid==0){ /*child*/
			if (dup2(fd, 1) == -1) { /* redirect standard output of the process to the output file*/
    			fprintf(stderr, "%s\n", strerror(errno));
    			close(fd);
    			exit(1);
    		}
			if (execvp(arglist[0], arglist) == -1) { /* execute command */
    			fprintf(stderr, "%s: %s\n", arglist[0], strerror(errno));
    			close(fd);
    			exit(1);
    		}
		}
		else{ /*parent*/
  			signal(SIGCHLD, SIG_IGN); /* zombie processes prevention */
			if(waitpid(pid, NULL, WUNTRACED)==-1 && errno!=EINTR && errno!=ECHILD){ /* waiting for the command (child process) to complete */
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1); 
			}
			close(fd);
			return 1;	
		}		
	}
	/* Executing command in the background */
	else if (!strcmp(arglist[count -1],"&")){
	
		arglist[count -1] = NULL; /* getting the command (before the "&" symbol) */
		
		pid_t pid =fork();
		
		if (pid==-1){ 
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		else if (pid==0){ /*child*/ 
			background_sig_pro();	/* Background child processes should not terminate upon SIGINT */
			if (execvp(arglist[0], arglist) == -1) { /* execute command in the background */
    			fprintf(stderr, "%s: %s\n", arglist[0], strerror(errno));
    			exit(1);
    		}
		}
		else{  /*parent*/
			signal(SIGCHLD, SIG_IGN); /* zombie processes prevention */
			return 1; /* The parent should not wait for the child process to finish, but instead continue executing commands.*/
		}
	}
	/* Executing command */
	else{
		pid_t pid =fork();
		
		if (pid==-1){ 
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		else if (pid==0) { /*child*/
		    
			if (execvp(arglist[0], arglist) == -1) { /* execute command */
    			fprintf(stderr, "%s: %s\n", arglist[0], strerror(errno));
    			exit(1);
    		}
    	}
		else{ /*parent*/
			signal(SIGCHLD, SIG_IGN); /* zombie processes prevention */
			if(waitpid(pid, NULL, WUNTRACED)==-1 && errno!=EINTR && errno!=ECHILD){ /* waiting for the command (child process) to complete */
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1); 
			}
			return 1;
		}
	}
	return 0;	
}

int finalize(void){
	return 0;
}
