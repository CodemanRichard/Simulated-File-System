#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

FILE *log_file;
int fd;
char* diskfile;

int current_cylinder_num = 0;
int next_cylinder_num;
unsigned long delay;

int portno, sockfd, newsockfd;
char request[256];

#define block_size 256
int fd;
int cylinders;
int sector_per_cylinder;
int track_to_track_delay;
char disk_storage_filename[100];

void error(char *errorMessage){
	printf("%s\n", errorMessage);
	return;
}

/*Connect the program to socket*/
void connect_to_socket();
/*Stimulate the disk arm moving*/
void disk_arm_moving();
/*Eat the '\n'*/
void clean_up();
/*Load necessary information from shell*/
void information_load(int argc, char* argv[]);
/*Connect the program to disk.log*/
void connect_to_log();
/*Disconnect the program to disk.log*/
void disconnect_to_log();
/*Write information to disk.log*/
void write_log(char* information_to_log);
/*Create a mapping between a memory space and a file*/
void file_mapping();
/*For I instruction, return number of cylinders and number of sectors per cylinder*/
void return_information();
/*For R instruction, read the contents*/
void read_contents();
/*For W instruction, write the contents*/
void write_contents();

int main(int argc, char* argv[])
{
    information_load(argc, argv);
    connect_to_log();
    file_mapping();
    connect_to_socket();

    int i;
    char info[256];
    char cylinders_string[256];
    char sector_per_cylinder_string[256];
    sprintf(cylinders_string, "%d", cylinders);
    sprintf(sector_per_cylinder_string, "%d", sector_per_cylinder);
    strncat(info, cylinders_string, strlen(cylinders_string));
    strncat(info, " ", 2);
    strncat(info, sector_per_cylinder_string, strlen(sector_per_cylinder_string));
    for(int i=strlen(info); i<256; i++)
        info[i] = '\0';

    while(1)
    {
        for(i=0; i<256; i++)
            request[i] = '\0';
        read(newsockfd, request, 256);

        fflush(stdin);
        printf("debug\n");
        printf("Receive: %s\n", request);

        if(strcmp(request, "Disk") == 0){
            printf("Connected to fs.c\n");
            write(newsockfd, info, strlen(info));
        }
        if(request[0] == 'I')
            return_information();
        else if(request[0] == 'R')
            read_contents();
        else if(request[0] == 'W')
            write_contents();
        else if(request[0] == 'E'){
            write_log("Goodbye");
            break;
        }
        else{
            continue;
        }
    }

    disconnect_to_log();
    close(fd);
    close(sockfd);
    return 0;
}

/*Connect the program to socket*/
void connect_to_socket()
{
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
		error("ERROR opening socket");
	bzero((char*)&serv_addr, sizeof(serv_addr));
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
    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
    if(newsockfd < 0)
        error("Error on accept!\n");
}

void disk_arm_moving()
{
    delay = track_to_track_delay * abs(current_cylinder_num - next_cylinder_num);
    usleep(delay);
    printf("Track to track time: %d us\n", (int)delay);
    return;
}

void clean_up()
{
    char buf;
    scanf("%c", &buf);   // eat '\n'
    while(buf != '\n')
        scanf("%c", &buf);
    return;
}

void information_load(int argc, char* argv[])
{
    cylinders = atoi(argv[1]);
    sector_per_cylinder = atoi(argv[2]);
    track_to_track_delay = atoi(argv[3]);
    strcpy(disk_storage_filename, argv[4]);
    portno = atoi(argv[5]);
    return;
}

void connect_to_log()
{
    log_file = fopen("disk.log", "w");
    if(log_file == NULL){
        printf("Error: Could not open file disk.log.\n");
        exit(-1);
    }
    return;
}

void disconnect_to_log()
{
    fclose(log_file);
    return;
}

void write_log(char* information_to_log)
{
    int information_len = strlen(information_to_log);
    fwrite(information_to_log, 1, information_len, log_file);
    return;
}

void file_mapping()
{
    fd = open(disk_storage_filename, O_RDWR | O_CREAT, 0);
    if(fd < 0){
        printf("Error: Could not open file '%s'.\n", disk_storage_filename);
        exit(-1);
    }

    // stretch the file size to the size of the simulated disk
    long FILESIZE = block_size * sector_per_cylinder * cylinders;
    int result = lseek(fd, FILESIZE-1, SEEK_SET);
    if(result == -1){
        perror("Error calling lseek() to 'stretch' the file");
        close(fd);
        exit(-1);
    }   

    // write something at the end of the file to ensure the file actually have the new size
    result = write(fd, "", 1);
    if(result != 1){
        perror("Error writing last byte of the file");
        close(fd);
        exit(-1);
    }

    // map the file
    diskfile = (char*)mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(diskfile == MAP_FAILED){
        close(fd);
        printf("Error: Could not map file.\n");
        exit(-1);
    }
}

void return_information()
{
    char information[4];
    information[0] = '0' + cylinders;
    information[1] = ' ';
    information[2] = '0' + sector_per_cylinder;
    //write_log(information);
    //write_log("\n\n");
    write(newsockfd, information, strlen(information));
    write(newsockfd, "\n\n", 3);
    clean_up();
    return;
}

// For R c s, output No if no such block exists
// otherwise, output Yes followed by a writespace and those 256 bytes of information
// c: cylinder;     s: sector
void read_contents()
{
    int i, index;
    int count = 0;
    int c, s;
    char c_string[20];
    char s_string[20];

    // Eat the blank
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            index = i + 1;
            break;
        }
    }
    // Read out c
    for(i=index; i<strlen(request); i++){
        if(request[i] == ' '){
            index = i + 1;
            break;
        }
        c_string[count] = request[i];
        count++;
    }
    c_string[count] = '\0';
    c = atoi(c_string);
    count = 0;
    // Read out s
    for(i=index; i<strlen(request); i++){
        s_string[count] = request[i];
        count++;
    }
    s_string[count] = '\0';
    s = atoi(s_string);
    
    // if no such block exists
    if(c>cylinders || s>sector_per_cylinder){
        //write_log("No\n\n");
        printf("Block doesn't exist\n");
        return;
    }

    next_cylinder_num = c;
    disk_arm_moving();
    current_cylinder_num = c;

    char buffer[256];
    memcpy(buffer, &diskfile[block_size*(c*sector_per_cylinder + s)], 256);
    //write_log("Yes\t");
    //write_log(buffer);
    //write_log("\n\n");
    printf("Data read out successfully\n");
    write(newsockfd, buffer, strlen(buffer));

    clean_up();

    return;
}

// For W c s data, Write Yes or No to show whether it is a valid write request or not
// c: cylinder;     s: sector
void write_contents()
{
    int i, index;
    int count = 0;
    int c, s;
    char c_string[20];
    char s_string[20];
    char data[256];

    // Eat the blank
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            index = i + 1;
            break;
        }
    }
    // Read out c
    for(i=index; i<strlen(request); i++){
        if(request[i] == ' '){
            index = i + 1;
            break;
        }
        c_string[count] = request[i];
        count++;
    }
    c_string[count] = '\0';
    c = atoi(c_string);
    count = 0;
    // Read out s
    for(i=index; i<strlen(request); i++){
        if(request[i] == ' '){
            index = i + 1;
            break;
        }
        s_string[count] = request[i];
        count++;
    }
    s_string[count] = '\0';
    s = atoi(s_string);
    count = 0;
    // Read out data
    for(i=index; i<strlen(request); i++){
        data[count] = request[i];
        count++;
    }
    data[count] = '\0';

    // if it is not a valid write request
    if(c>cylinders || s>sector_per_cylinder){
        clean_up();
        printf("Instruction error!\n");
        write_log("No\n\n");
        return;
    }

    next_cylinder_num = c;
    disk_arm_moving();
    current_cylinder_num = c;

    int len = strlen(data);
    memcpy(&diskfile[block_size*(c*sector_per_cylinder+s)], data, len);
    write_log("Yes\n\n");

    clean_up();

    return;
}