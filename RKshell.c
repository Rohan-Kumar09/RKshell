#include <stdio.h> // for printf, scanf, perror
#include <unistd.h> // for read, write, fork, execvp
#include <sys/types.h> // for pid_t
#include <stdlib.h> 
#include <string.h> 
#include <signal.h> // for signal
#include <sys/wait.h> // for waitpid

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

/**
* setup() reads in the next command line, separating it into distinct tokens
* using whitespace as delimiters. setup() sets the args parameter as a
* null-terminated string.
*/

pid_t foreground_pid = -1; // global variable to store the PID of the foreground process

void handle_SIGQUIT(int signum){
    if(foreground_pid != -1){
        printf("Caught <control> <\\> signal\n");
        kill(foreground_pid, SIGQUIT); // send SIGQUIT to the foreground process
    }
}

void sigHandler(void){
    struct sigaction handler;
    handler.sa_handler = handle_SIGQUIT;
    handler.sa_flags = SA_RESTART;
    sigaction(SIGQUIT, &handler, NULL);
    /* when <control> <\>  is called */
}

void setup(char inputBuffer[], char *args[],int *background) {
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
   
    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE); 

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */
    if (length < 0){
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    /* examine every character in the inputBuffer */
    for (i=0;i<length;i++) {
        switch (inputBuffer[i]){
          case ' ':
          case '\t' :               /* argument separators */
            if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                ct++;
            }
            inputBuffer[i] = '\0'; /* add a null char; make a C string */
            start = -1;
            break;
          case '\n':                 /* should be the final char examined */
            if (start != -1){
                    args[ct] = &inputBuffer[start];    
                ct++;
            }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
            break;
          default :             /* some other character */
            if (start == -1)
                start = i;
            if (inputBuffer[i] == '&'){
                *background  = 1;
                start = -1;
                inputBuffer[i] = '\0';
            }
        }
    }   
    args[ct] = NULL; /* just in case the input line was > 80 */
}

int makeFork(char *args[], int background){
    pid_t pid;
    pid = fork();
    if(pid < 0){ // if the fork fails
        fprintf(stderr, "fork failed");
        return 1;
    }
    else if (pid == 0){ // if the fork is successful
        if (execvp(args[0], args) == -1){ // child process
            fprintf(stderr, "execvp failed\n"); // execvp returns only on error
            exit(EXIT_FAILURE);
        }
    }
    else{ // wait for the child process to finish
        if (background == 1) { // process should run in background
            printf("[Child PID = %d, background = TRUE]\n", pid);
            // kill(pid, SIGCONT); // stop the process with the given pid
            return pid;
        }
        else{ // process should run in foreground
            printf("[Child PID = %d], background = FALSE]\n", pid); // Print the child process ID
            foreground_pid = pid; // store the PID of the foreground process
            waitpid(pid, NULL, 0); // wait for the child process to finish
            // kill(pid, SIGCONT); // resume the process with the given pid
            printf("Child process complete.\n");
            foreground_pid = -1; // reset the PID of the foreground process
        }
    }
    return pid;
}

int main(void) {
    char inputBuffer[MAX_LINE];      /* buffer to hold the command entered */
    int background;              /* equals 1 if a command is followed by '&' */
    char *args[(MAX_LINE/2)+1];  /* command line (of 80) has max of 40 arguments */

    printf("Welcome to RKshell. ");
    int pid = getpid(); // get the process id of the shell
    printf("PID: %d\n", pid); // print the process id of the shell
    sigHandler(); // set up the signal handler
    int i = 0;
    while (1){            /* Program terminates normally inside setup */
        i++; // command number counter
        background = 0;
        printf("RKshell[%d] $ ", i);
        fflush(stdout);
        setup(inputBuffer,args,&background);       /* get next command */
        // handle built-in commands
        if(args[0] == NULL){ // if no command is entered
            continue;
        }
        else if(strcmp(args[0], "exit") == 0){
            printf("RKshell exiting\n");
            exit(0);
        }
        else if(strcmp(args[0], "bg") == 0){ // if the command is "bg"
            kill(atoi(args[1]), SIGCONT); // resume the process with the given pid
        }
        else if(strcmp(args[0], "fg") == 0){
            kill(atoi(args[1]), SIGCONT); // resume the process with the given pid
            foreground_pid = atoi(args[1]);
            waitpid(atoi(args[1]), NULL, 0); // wait for the process to finish
            printf("Child Complete: pid = %d\n", atoi(args[1]));
        }
        else if(strcmp(args[0], "jobs") == 0){ // if the command is "jobs"
            system("ps -o pid,tty,stat,time,comm"); // print the list of all processes
        }
        else if (strcmp(args[0], "kill") == 0){ // if the command is "kill"
            kill(atoi(args[1]), SIGKILL); // kill the process with the given pid
        }
        else if(strcmp(args[0], "stop") == 0){ // if the command is "stop"
            kill(atoi(args[1]), SIGSTOP); // stop the process with the given pid
        }
        else{ // if not a built-in command
            pid = makeFork(args, background);
        }
    }
}