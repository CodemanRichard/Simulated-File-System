#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <netdb.h>

void error(char *errorMessage){
	printf("%s\n", errorMessage);
	return;
}

int main(int argc, char* argv[])
{
	int n;
	char buffer[256];
	char command[256];
	int sockfd;
	char cleaner;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	struct hostent *host;
	//new
	bzero((char *) &serv_addr, sizeof(serv_addr));
	//new
	serv_addr.sin_family = AF_INET;
	host = gethostbyname(argv[1]);
	//memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
	//new
	bcopy((char *)host->h_addr, (char *)&serv_addr.sin_addr.s_addr, host->h_length);
	//new
	serv_addr.sin_port = htons(atoi(argv[2]));
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("Error connecting!\n");

	while(1){
		printf("Please enter your command: ");
		scanf("%[^\n]", command);
		scanf("%c", &cleaner);

		n = write(sockfd, command, strlen(command));
		if(n < 0)
			error("Error writing to socket");
		n = read(sockfd, buffer, 255);
		if(n < 0)
			error("Error reading from socket");
		
		if(strcmp(buffer, "Goodbye!") == 0){
			printf("Goodbye!\n");
			break;
		}
		else if(strcmp(command, "ls") == 0){
			while(strcmp(buffer, "last") != 0){
				printf("%s\n", buffer);
				read(sockfd, buffer, 255);
			}
		}
		else{
			printf("%s\n", buffer);
		}
	}
	close(sockfd);
	return 0;
}