#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(char *errorMessage){
	printf("%s\n", errorMessage);
	return;
}

// Parsing command
int parseLine(char *line, char **command_array){
	char *p;
	int count = 0;
	line[strlen(line)-2] = '\0';	// erase the line break
	p = strtok(line, " ");
	while(p && strcmp(p, "|") != 0){
		command_array[count] = p;
		count = count + 1;
		p = strtok(NULL, " ");
	}
	command_array[count] = NULL;
	return count;
}

void dostuff(int sock){
	int n, pid;
	char buffer[256];
	char *command_array[256];
	while(1){
		bzero(buffer, 256);
		n = read(sock, buffer, 255);
		if(n < 0)
			error("ERROR reading from socket");
		printf("Here is the message: %s\n", buffer);
		n = write(sock, "I got your message\n", 20);
		if(n < 0)
			error("ERROR writing to socket");
		// parseline and execvp	
		n = parseLine(buffer, command_array);
		//---test---
		int mypipe[2];
		if(pipe(mypipe)){
			fprintf(stderr, "Pipe failed.\n");	
		}
		//---test---
		pid = fork();
		if(pid == -1)
			printf("Error on fork");
		else if(pid == 0){
			//---test---
			close(mypipe[0]);
			close(1);
			dup2(mypipe[1], STDOUT_FILENO);
			//close(mypipe[1]);
			//--test---
			if(execvp(command_array[0], command_array)==-1)
				printf("Error: running command: %s\n", buffer);	
			close(mypipe[1]);
		}
		// parent process doesn't do anything
		//---test---
		else{
			wait(NULL);
			close(mypipe[1]);
			close(0);
			//dup2(mypipe[0], STDIN_FILENO);
			//close(mypipe[0]);
			n = read(mypipe[0], buffer, 256);
			while(n>0){
				write(sock, buffer, n);
				n = read(mypipe[0], buffer, 256);
			}
			close(mypipe[0]);
		}
		//---test---	
	}
	return;
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;	
	if(argc < 2){
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
		error("ERROR opening socket");
	bzero((char*)&serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;		// specify that this is an PIv4 address
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// bind the socket to all available network interfaces
	serv_addr.sin_port = htons(portno);	// set the port number of this structure to portno
						// the 'htons()' function converts the port number from host byte order to network byte order, which is necessary for network communication
	if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
		error("ERROR on binding");
	listen(sockfd, 5);
	if(listen(sockfd, 5) == -1){
		error("listen failed");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	clilen = sizeof(cli_addr);
	while(1){
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if(newsockfd < 0)
			error("ERROR on accept");
		pid = fork();
		if(pid < 0)
			error("ERROR on fork");
		if(pid == 0){
			close(sockfd);
			dostuff(newsockfd);
			exit(0);
		}
		else close(newsockfd);	
	}
	close(sockfd);
	return 0;	// never get here
}
