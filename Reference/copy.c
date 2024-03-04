// This program is to use pipeline and fork() and copy contents from one file to another
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/wait.h>

int main(int argc, char* argv[])
{
	clock_t start, end;
	const int bufferSize = atoi(argv[3]);	
	char buffer[bufferSize];	// bufferSize is written successfully
	double elapsed;
	start = clock();

	// Open a file for reading and check for errors
	FILE *src;
	src = fopen(argv[1],"r");       // Open source file, "r": Opens a source file for reading
	if(src == NULL){        // Check for error
        	printf("Error: Could not open file '%s'.\n", argv[1]);
        	exit(-1);
	}

	// get the size of source file
	fseek(src, 0L, SEEK_END);
	int fileSize = ftell(src);
	rewind(src);

	// Open a file for writing and check for errors
	FILE *dest;
	dest = fopen(argv[2], "w+");	// Open destination file, "w+": Opens an empty file for both reading and writing. If the given file exists, its contents are destroyed
	if(dest == NULL){	//Check for file error
		printf("Error: Could not open file '%s'.\n", argv[2]);
		fclose(src);
		exit(-1);
	}

	int mypipe[2];
	if(pipe(mypipe)){
		fprintf(stderr, "Pipe failed.\n");
		return -1;
	}

	pid_t ForkPID;
	ForkPID = fork();

	switch(ForkPID){
		case -1:
			printf("Error: Failed to fork.\n"); break;
		case 0:		// Child process set as writer
			close(mypipe[0]);
			while(fread(buffer, 1, bufferSize, src)==bufferSize){
				write(mypipe[1], buffer, bufferSize);
			}	
			write(mypipe[1], buffer, bufferSize);	
			close(mypipe[1]);
			break;
		default:	// Parent process set as reader
			close(mypipe[1]);
			wait(NULL);	// wait for child process to finish sending characters to pipe
			while(read(mypipe[0], buffer, bufferSize)>0){
				fwrite(buffer, 1, bufferSize, dest);
			}
			close(mypipe[0]);	
	}	

	end = clock();
	elapsed = ((double)(end-start)) / CLOCKS_PER_SEC*1000;
	printf("Time used: %f millisecond\n", elapsed);
	
	return 0;	
}
