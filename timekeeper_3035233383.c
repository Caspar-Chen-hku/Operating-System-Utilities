/************************************************************
* Filename: timekeeper_3035233383.c
* Student name: Chen Cheng
* Student no.: 3035233383
* Development platform: Ubuntu
* Compilation: gcc timekeeper_3035233383.c –o timekeeper
* Remark: Complete all 3 stages
*************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <signal.h>

#define ENOUGH 20

//handler of the SIGINT signal of parent process, which just does nothing
void sigint_handler(int signum) {
        return;
}

//string declared to match signal number to signal name
char *signame[]={"INVALID", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGPOLL", "SIGPWR", "SIGSYS", NULL};

//executes the command by the child process
void perform(char** commands){     
        int returncode = execvp(commands[0],commands);
        if (returncode!=-1){
		printf("execution of command %s finishes successfully\n", commands[0]);
        } else {
		printf("timekeeper experienced an error in starting the command: %s\n", commands[0]);
	}
}

//print commands, mainly for debugging purpose
void printcommands(char*** commands, int numcmd, int* numargs){
	int i,j;
	printf("number of commands: %d\n",numcmd);
	printf("the commands are:\n");
	for (i=0; i<numcmd; i++){
		for (j=0; j<numargs[i]; j++){
			printf("%s ",commands[i][j]);
		}
		printf(";\n");
	}
}

//get number of commands
int getnumcmd(int argc, char** argv){
        int numcmd=1;
	int i;
        for (i=1; i<argc; i++){
		if (argv[i][0]=='!'){
			numcmd++;
		}
        }
	return numcmd;
}

//get number of arguments for each command
int* getnumargs(int argc, char** argv, int numcmd){
        int *numargs;
        numargs = malloc(numcmd * sizeof(int));
	
	int i;
        int currentnum = 0;
        int currentindex = 0;
        for (i=1; i<argc; i++) {
        	if (argv[i][0]!='!'){
			currentnum++;
		}else{
			numargs[currentindex]=currentnum;
   			currentnum=0;
              		currentindex++;
		}
        }
 	numargs[numcmd-1] = currentnum;
	return numargs;
}

int main(int argc, char** argv) {

        //if no argument is provided, the program should exit without any output
        if (argc==1){
                return 0;
        }

        //parse input into a 2D array of strings
        char ***commands;
        int numcmd = getnumcmd(argc, argv);
        commands = malloc(numcmd * sizeof(char**));
	int* numargs = getnumargs(argc,argv,numcmd);

	//fill in the value of commands
	int i,j;
	int currentindex = 1;
	for (i = 0; i < numcmd; i++) {
    		commands[i] = malloc(numargs[i] * sizeof(char*));
		for (j = 0; j < numargs[i]; j++){
			commands[i][j] = malloc(30*sizeof(char));
			if (argv[currentindex][0]=='!'){
				currentindex++;
			}
			commands[i][j] = argv[currentindex];
			currentindex++;
		} 
        }	
	printcommands(commands,numcmd,numargs);

	//make the parent ignore sigint
        signal(SIGINT, sigint_handler);

        //define the structures that will be used to store realtime
        struct timespec* tp1 = malloc(numcmd*sizeof(struct timespec));
	struct timespec* tp2 = malloc(numcmd*sizeof(struct timespec));
	siginfo_t infop;

	//create pipes
	int** pfd = malloc((numcmd-1)*sizeof(int*));
	for (i=0; i<numcmd-1; i++){
		pfd[i] = malloc(2*sizeof(int));
		pipe(pfd[i]);
	} 
	
	//create child processes and make them execute specific commands
	for (i=0; i<numcmd; i++){
        	//store the time when child process starts
        	clock_gettime(CLOCK_MONOTONIC, &tp1[i]);
       		pid_t forkpid = fork();
	
		if (forkpid < 0) {
			printf("fork: error no = %s\n", strerror(errno));
			exit(-1);
		} else if (forkpid == 0) {
                	printf("\n");
			int index,rw;
			//connect the pipes accordingly, close unused pipe
			if (numcmd>=2){
				for (index=0; index<numcmd-1; index++){
					for (rw=0; rw<2; rw++){
						if ((index==i-1&&rw==0)||(index==i&&rw==1)){
							continue;
						}
						close(pfd[index][rw]);
					}
				}
				if (i==0){
					dup2(pfd[0][1], 1);
				} else if (i==numcmd-1){
					dup2(pfd[numcmd-2][0], 0);
				} else {
					dup2(pfd[i-1][0], 0);
					dup2(pfd[i][1], 1);
				}
			}
			//execute the desired command	 
			perform(commands[i]);
		} else {
			//print information about the created child
			printf("Child process has been created with pid %d for the command %s \n", (int) forkpid, commands[i][0]);
		}
	}

	int index,rw;
	//close all the pipes for parent
	if (numcmd>=2){
		for (index=0; index<numcmd-1; index++){
			for (rw=0; rw<2; rw++){
			 	close(pfd[index][rw]);
			}
		}
	}

	//declare necessary data
	pid_t childpid;
	index=0;
	FILE *fp;
	char filename[ENOUGH];
	char pidstr[ENOUGH];
	int redundint, uTime, sTime;
	char redundchar;

	//wait for any children to terminate
	while (waitid(P_ALL, (id_t) 1 , &infop, WEXITED|WNOWAIT)!=-1){
		//store the time when child process ends
               	clock_gettime(CLOCK_MONOTONIC, &tp2[index]);

 		childpid = infop.si_pid;
		strcpy(filename,"/proc/");
			 
		//compose filename that will be read
		//this file contains the uTime and sTime info
        	sprintf(pidstr, "%d", (int)childpid);
		strcat(filename, pidstr);
		strcat(filename, "/stat");

   		fp = fopen(filename, "r"); // read mode
 
   		if (fp == NULL)
   		{
      			perror("Error while opening the file.\n");
      			exit(EXIT_FAILURE);
   		}

		for (i=0; i<13; i++){
			redundchar = fgetc(fp);
		}
		//get the utime and stime from the file
		redundchar = fgetc(fp);
		uTime = atoi(&redundchar);
		redundchar = fgetc(fp);
		sTime = atoi(&redundchar);
 
   		fclose(fp);

        	//read the file that contains context switch info
		strcpy(filename,"/proc/");
		strcat(filename, pidstr);
		strcat(filename, "/status");

   		fp = fopen(filename, "r"); // read mode
 
   		if (fp == NULL)
   		{
      			perror("Error while opening the file.\n");
      			exit(EXIT_FAILURE);
   		}

		//declare the string to store the read content
		char line[2*ENOUGH];
		char vswitch[ENOUGH];
		char nswitch[ENOUGH];
		while(fgets(line, 2*ENOUGH, fp)) {
			if (strncmp(line,"voluntary_ctxt_switches:", 24) == 0){
   				strcpy(vswitch,line+25);
			}else if (strncmp(line,"nonvoluntary_ctxt_switches:", 27) == 0){
   				strcpy(nswitch,line+28);
			}
		}
		
		//add voluntary and nonvoluntary together to get number of switches
		int numswitch = atoi(vswitch) + atoi(nswitch);
 
   		fclose(fp);		
	
               	//calculate the realtime that the child process spent to complete
               	double realtime = (double) ( tp2[index].tv_sec - tp1[index].tv_sec ) + ( tp2[index].tv_nsec - tp1[index].tv_nsec )/1000000000.0; 
         
		//print different info according to different termination code   
		int sig = infop.si_status;
		if (infop.si_code==CLD_EXITED){
			printf("\nChild process (%d) of command %s exited, with exit status code %d\n", (int) childpid, commands[index][0], sig);
		}else if (infop.si_code==CLD_KILLED){
			printf("The command %s is killed by the signal number = %d (%s)\n",commands[index][0],sig,signame[sig]);
		}else if (infop.si_code==CLD_DUMPED){
			printf("The command %s is killed by the signal number = %d (%s) with a dumped core\n",commands[index][0],sig,signame[sig]);
		}else if (infop.si_code==CLD_STOPPED){
			printf("The command %s is stopped by the signal number = %d (%s)\n",commands[index][0],sig,signame[sig]);
		}else {
			printf("The traced command %s is trapped by the signal number = %d (%s)\n",commands[index][0],sig,signame[sig]);
		}

               	//print the statistics
               	printf("real: %.2f s, user: %.2f s, system: %.2f s, context switch: %d\n", realtime, (double) uTime*1.0/sysconf(_SC_CLK_TCK), (double) sTime*1.0/sysconf(_SC_CLK_TCK) , numswitch);
		index++;

		//totally terminate the process
		waitpid(childpid, NULL, 0);
	}
	return 0;
}
