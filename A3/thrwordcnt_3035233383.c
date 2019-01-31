/************************************************************
* Filename: thrwordcnt_3035233383.c
* Student name: Chen Cheng
* Student no.: 3035233383
* Development platform: Oracle VM VirtualBox
* Compilation: gcc thrwordcnt_3035233383.c -o thrwordcnt -Wall -pthread
* Remark: All done
*************************************************************/

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <signal.h>
#include <ctype.h>

#define ENOUGH 50
#define MAXLEN	116

//these variables will be shared by all threads
char** buffer;  //buffer to store the keywords to search for
sem_t mutex;    // mutex to ensure mutually exclusive access to buffer
sem_t notFull;  //condition variable to indicate buffer not full
sem_t notEmpty; // condition variable to indicate buffer not empty
int numWorker, numBuffer; //number of worker threads and number of buffer slots
char* target; //the target file name to search in
char* keywords; //the keyword file name
int nextToPut = 0; //the index of buffer to put the next keyword in
int nextToGet = 0; //the index of buffer to get the next keyword from
int searched = 0; //number of already searched keywords
int numKeyword; //total number of keywords
int* tasks;     //array to store number of tasks done by every worker thread
char** results; //array to store all the searching results

//pre: input is a C string to be converted to lower case
//post: return the input C string after converting all the characters to lower case
char * lower(char * input){
	unsigned int i;	
	for (i = 0; i < strlen(input); ++i)
		input[i] = tolower(input[i]);
	return input;
}

//pre: the keyword to search for
//post: the frequency of occurrence of the keyword in the file
int search(char * keyword){
	int count = 0;
	FILE * f;
	char * pos;
	char input[MAXLEN];
	char * word = strdup(keyword);		//clone the keyword	

	lower(word);	//convert the word to lower case
	f = fopen(target, "r");		//open the file stream

    while (fscanf(f, "%s", input) != EOF){  //if not EOF
        lower(input);	//change the input to lower case
        if (strcmp(input, word) == 0)	//perfect match
			count++;
		else {
			pos = strstr(input, word); //search for substring
			if (pos != NULL) { //keyword is a substring of this string
				if ((pos - input > 0) && (isalpha(*(pos-1)))) continue; 
				if (((pos-input+strlen(word) < strlen(input))) 
					&& (isalpha(*(pos+strlen(word))))) continue;
				count++;  
			}
		}
    }
    fclose(f);
	free(word);	
    return count;
}

//pre: input is the thread id of the thread
//post: increament the tasks[id] value, store the result into results array
//worker threads call this to accomplish searching
void * threadSearch(void * a)
{
    int id = *(int *)a;
    printf("Worker(%d) : Start up. Wait for task!\n",id);

    //enable itself to be canceled
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    char result[ENOUGH];
    char countstr[ENOUGH];
    while(searched<numKeyword){

	sem_wait(&notEmpty);
    	sem_wait(&mutex);

	printf("Worker(%d) : search for keyword \"%s\"\n",id,buffer[nextToGet]);

    	int count = search(buffer[nextToGet]);

	strcpy(result,buffer[nextToGet]);
	strcat(result, " : ");
	sprintf(countstr, "%d", count);
	strcat(result,countstr);
	strcpy(results[searched],result);
    	nextToGet = (nextToGet+1)%numBuffer;
    	searched++;

	tasks[id] = tasks[id]+1;

    	sem_post(&mutex);
    	sem_post(&notFull);
    }
    pthread_exit(NULL);
}

int main(int argc, char * argv[])
{
    //parse the arguments
    if (argc<5){
	printf("Usage: ./thrwordcnt [number of workers] [number of buffers] [target plaintext file] [keyword file]\n");
	return 0;
    }
    numWorker = atoi(argv[1]);
    numBuffer = atoi(argv[2]);
    if (numWorker<1 || numWorker>15){
	printf("The number of worker threads must be between 1 to 15\n");
	return 0;
    }else if(numBuffer<1 || numBuffer>10){
	printf("The number of buffers in task pool must be between 1 to 10\n");
	return 0;
    }
    target = malloc(ENOUGH*sizeof(char));
    keywords = malloc(ENOUGH*sizeof(char));
    strcpy(target,argv[3]);
    strcpy(keywords,argv[4]);
    buffer = malloc(numBuffer*sizeof(char*));

    //initialize the buffer and threads
    int i;
    for (i=0; i<numBuffer; i++){
	buffer[i] = malloc(ENOUGH*sizeof(char));
    }

    pthread_t* threads = malloc(numWorker * sizeof(pthread_t));

    //initialize the tasks array
    tasks = malloc(numWorker * sizeof(int));
    for (i=0; i<numWorker; i++){
	tasks[i]=0;
    }
    
    //initialize access semaphore
    if (sem_init(&mutex, 0, 1) == -1)
    {
	    printf("Error initialize the semaphore\n");
	    exit(1);
    }
    //initialize notFull and notEmpty
    if (sem_init(&notFull, 0, numBuffer) == -1)
    {
	    printf("Error initialize the semaphore\n");
	    exit(1);
    }
    if (sem_init(&notEmpty, 0, 0) == -1)
    {
	    printf("Error initialize the semaphore\n");
	    exit(1);
    }

    FILE * k = fopen(keywords, "r");
    fscanf(k, "%d", &numKeyword);		//read in the number of keywords
    char word[ENOUGH];	

    //initialize results array
    results = malloc(numKeyword*sizeof(char*));
    for (i=0; i<numKeyword; i++){
	results[i] = malloc(ENOUGH*sizeof(char));
    }

    //initialize the array to store ids
    int ids[numWorker];
    for (i=0; i<numWorker; i++){
	ids[i] = i;
    }
    //create and execute worker threads
    for (i=0; i<numWorker; i++){
	sem_wait(&mutex);
    	if(pthread_create(&threads[i], NULL, threadSearch, (void *)&ids[i]))
    	{
    		printf("ERROR creating thread 1\n");
    		exit(1);
    	}
	sem_post(&mutex);	
    }

    //read keyword one by one and put into buffer
    for (i = 0; i < numKeyword; ++i) {
		sem_wait(&notFull);
		sem_wait(&mutex);

		fscanf(k, "%s", word);
		strcpy(buffer[nextToPut],word);
		nextToPut = (nextToPut + 1) % numBuffer;

		sem_post(&mutex);
		sem_post(&notEmpty);
    }

    fclose(k);

    //wait for all threads to terminate, cancel all the blocking threads
    while(1){
    	if(searched>=numKeyword){ 
		for (i=0; i<numWorker; i++){
			pthread_cancel(threads[i]);
		}
    		break; 
    	}
    	sleep(0.5);
    }

    //print out the working statistics and searching results    
    for (i=0; i<numWorker; i++){	
	printf("Worker thread %d has terminated and completed %d jobs\n",i,tasks[i]);
    }

    for (i=0; i<numKeyword; i++){
	printf("%s\n",results[i]);
    }

    //destroy the semaphores        
    sem_destroy(&mutex);
    sem_destroy(&notFull);
    sem_destroy(&notEmpty);

    //free all the allocated memory
    free(target);
    free(keywords);
    free(buffer);
    free(threads);
    free(tasks);
    free(results);
    

}

