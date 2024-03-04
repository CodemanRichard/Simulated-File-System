// 参考操作系统课本285页的内容，对比本项目的实现和标准实现的差距
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include <spinlock>
#include <linux/rcupdate.h>

#define BLOCK_NUM 256
#define BLOCK_SIZE 256
#define INODE_NUM 128
//#define INODE_SIZE 128
#define IS_FILE 1
#define IS_DIR 2
#define IS_EMPTY 0
#define IS_FULL 1

typedef struct
{
    int inodes_count;
    int blocks_count;
    int free_inodes_count;
    int free_blocks_count;
    int root;
    int curDirIndex;    // to inode
}SuperBlock;

typedef struct
{
    char data[BLOCK_SIZE];
}Block;

typedef struct
{
    int file_size;
    int file_type;
    char file_name[20];
    time_t creation_time;
    time_t last_modification_time;
    int direct_block[4];    // to store the index in blocks
    int single_indirect_link;   // each link leads to 8 blocks
    int double_indirect_link;   // each link leads to 8 blocks
    int next_available_block;   // logical block in inode
    int last_directory_index;
    int index;
}Inode;

int portno, sockfd, newsockfd, serverPort, client_sockfd; // For server
struct hostent *host;   // For client
int cylinders, sector_per_cylinder;
std::mutex mtx;

FILE *log_file;
SuperBlock mySuperBlock;
Inode inodes[INODE_NUM];
Block blocks[BLOCK_NUM];
int emptyInodes[INODE_NUM];
int emptyBlocks[BLOCK_NUM];

void error(char *errorMessage){
	printf("%s\n", errorMessage);
	return;
}

void make_empty(char *myString){
    for(int i=0; i<strlen(myString); i++)
        myString[i] = '\0';
    return;
}

/*Connect the program to fs.log*/
void connect_to_log();
/*Disconnect the program to fs.log*/
void disconnect_to_log();
/*Write information to fs.log*/
void write_log(char* information_to_log);

/* The initialization of SuperBlock */
void superBlockInit();
/* The initialization of home directory */
void homeDirInit();
/* The initialization of a file */
void fileInit(Inode *curDir, int inode_index, int physical_block_index, int logical_block_index, char *request);
/* The initialization of a directory */
void dirInit(Inode *curDir, int inode_index, int physical_block_index, int logical_block_index, char *request);

/* Look for the file in double_indirect_link in inode, return -1 if not found, return the block index which stores the inode index if found */
int double_indirect_search(Inode *curDir, char *name);
/* Look for the file in single_indirect_link in inode, return -1 if not found, return the block index which stores the inode index if found */
int single_indirect_search(Inode *curDir, char *name);
/* Look for the file in the direct_blocks in inode, return -1 if not found, return the block index which stores the inode index if found */
int direct_search(Inode *curDir, char *name);
/* Display the data of the file destFile */
void display_data(Inode *destFile);
/* Overwrite data with length of dataLen in destFile */
void overwrite(Inode *destFile, int dataLen, char *data);
/* Insert to destFile at the position before the pos-th character(0-indexed), with the dataLen bytes of data */
void insert_data(Inode *destFile, int pos, int dataLen, char *data);
/* Delete the contents in destFile from the pos character(0-indexed), delete l bytes or till the end of the file*/
void delete_data(Inode *destFile, int pos, int dataLen);
/* Shift left every character in data by one spot */
void left_shift(char *data);

/* Format the file system on the disk */
// > f
void format_system();
/* Create a file named f in the file system */
// > mk f
void create_file(char *request);
/* Create a subdirectory named d in the currect directory */
// > mkdir d
void create_directory(char *request);
/* Delete the file named f from current directory*/
// > rm f
void delete_file(char *request);
/* Change the current working directory to the path */
// > cd path
void change_directory(char *request);
/* Delete the directory named d in the current directory */
// > rmdir d
void delete_directory(char *request);
/* Return a listing of the files and directories in the current directory */
// > ls
void directory_listing();
/* Read the file named f, and return the data that came from it */
// > cat f
void catch_file(char *request);
/* Overwrite the contents of the file named f with the l bytes of data */
// > w f l data
void write_file(char *request);
/* Insert to the file at the position before the pos-th character(0-indexed), with the l bytes of data */
// > i f pos l data
void insert_to_file(char *request);
/* Delete the contents from the pos character(0-indexed), delete l bytes or till the end of the file */
// > d f pos l
void delete_in_file(char *request);

/* Doing the work of server */
void serverWork();
/* Doing the work of client */
void clientWork();

int main(int argc, char* argv[])
{
    /******************************/
    /****Dealing with socket...****/
    /******************************/
    host = gethostbyname(argv[1]);
    serverPort = atoi(argv[2]);
    portno = atoi(argv[3]);

    // Do the connection to disk system
    clientWork();

    // Client needs to first get the information about disk
    char info[64];
    write(client_sockfd, "Disk", 5);
    read(client_sockfd, info, 64);
    printf("Receive data about disk.c: %s\n", info);
    char tmp;
    char cylinders_string[64];
    char sector_per_cylinder_string[64];
    // first read out cylinders
    int pointer = 0;
    int count = 0;
    tmp = info[pointer];
    while(tmp!=' '){
        cylinders_string[count] = tmp;
        pointer++;
        count++;
        tmp = info[pointer];
    }
    cylinders = atoi(cylinders_string);
    // then read out sector_per_cylinder
    count = 0;
    pointer++;
    tmp = info[pointer];
    while(tmp!='\0'){
        sector_per_cylinder_string[count] = tmp;
        pointer++;
        count++;
        tmp = info[pointer];
    }
    sector_per_cylinder = atoi(sector_per_cylinder_string);

    char request[256];  // The whole request
    char token[20];
    int i, lastPos;
	while(1){
        int sock = serverWork();
        // Get the request
        make_empty(request);
        make_empty(token);
        if(read(sock, request, 256) < 0)
            error("Error on reading\n");
        printf("Receive: %s\n", request);
        // Get the token
        for(i=0; i<strlen(request); i++){
            if(request[i] == ' ' || request[i] == '\0'){
                token[i] = '\0';
                break;
            }
            token[i] = request[i];
        }
        lastPos = i;
        for(i=lastPos; i<strlen(token); i++)
            token[i] = '\0';

        if(strcmp(token, "f") == 0){
            printf("Formatting system\n");
            std::thread t(format_system, sock);
            t.detach();
        }
        else if(strcmp(token, "mk") == 0){
            printf("Creating file\n");
            std::thread t(create_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "mkdir") == 0){
            printf("Creating directory\n");
            std::thread t(create_directory, sock, request);
            t.detach();
        }
        else if(strcmp(token, "rm") == 0){
            printf("Deleting file\n");
            std::thread t(delete_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "cd") == 0){
            printf("Changing directory\n");
            std::thread t(change_directory, sock, request);
            t.detach();
        }
        else if(strcmp(token, "rmdir") == 0){
            printf("Deleting directory\n");
            std::thread t(delete_directory, sock, request);
            t.detach();
        }
        else if(strcmp(token, "ls") == 0){
            printf("Listing directory\n");
            std::thread t(directory_listing, sock);
            t.detach();
        }
        else if(strcmp(token, "cat") == 0){
            printf("Catching the file\n");
            std::thread t(catch_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "w") == 0){
            printf("Writing file\n");
            std::thread t(write_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "i") == 0){
            printf("Inserting to file\n");
            std::thread t(insert_to_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "d") == 0){
            printf("Deleting in file\n");
            std::thread t(delete_in_file, sock, request);
            t.detach();
        }
        else if(strcmp(token, "e") == 0){
            printf("The other side has been closed");
            write(sock, "Goodbye!\n", 10);
            break;
        }
        else{
            printf("Invalid request\n");
            continue;
        }
	}
    close(sockfd);

    return 0;   // never get here
}

void connect_to_log()
{
    log_file = fopen("fs.log", "w");
    if(log_file == NULL){
        printf("Error: Could not open file fs.log.\n");
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

void superBlockInit()
{
    mySuperBlock.inodes_count = INODE_NUM;
    mySuperBlock.blocks_count = BLOCK_NUM;
    mySuperBlock.free_inodes_count = INODE_NUM;
    mySuperBlock.free_blocks_count = BLOCK_NUM;
    mySuperBlock.root = 0;  // home directory is the first inode
    mySuperBlock.curDirIndex = 0;
    
    int i;
    for(i=0; i<INODE_NUM; i++)
        emptyInodes[i] = IS_EMPTY;
    for(i=0; i<BLOCK_NUM; i++)
        emptyBlocks[i] = IS_EMPTY;

    return;
}

void homeDirInit()
{
    mySuperBlock.free_inodes_count--;
    emptyInodes[mySuperBlock.root] = IS_FULL;
    Inode *homeDirectory = &inodes[mySuperBlock.root];
    homeDirectory->file_size = 0;
    homeDirectory->file_type = IS_DIR;
    time(&(homeDirectory->creation_time));
    time(&(homeDirectory->last_modification_time));
    homeDirectory->next_available_block = 0;
    homeDirectory->last_directory_index = -1;
    homeDirectory->index = 0;
    return;
}

/* The initialization of a file */
void fileInit(Inode *curDir, int inode_index, int physical_block_index, int logical_block_index, char *request)
{
    // State update
    mySuperBlock.free_inodes_count--;
    mySuperBlock.free_blocks_count--;
    emptyInodes[inode_index] = IS_FULL;
    emptyBlocks[physical_block_index] = IS_FULL;
    curDir->next_available_block++;

    // New inode for file created
    Inode *fileNode = &inodes[inode_index];
    fileNode->file_size = 0;
    fileNode->file_type = IS_FILE;
    //scanf("%s", fileNode->file_name);
    time(&(fileNode->creation_time));
    time(&(fileNode->last_modification_time));
    fileNode->next_available_block = 0;
    fileNode->last_directory_index = curDir->last_directory_index;
    fileNode->index = inode_index;
    // Get the file name
    int i, startPoint, count = 0;
    char fileName[20];
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    for(i=startPoint; i<strlen(request); i++){
        fileName[count] = request[i];
        count++;
    }
    strcpy(fileNode->file_name, fileName);

    // Create the connection between curDir and fileNode
    // Use direct block
    if(logical_block_index < 4){
        curDir->direct_block[logical_block_index] = physical_block_index;
        char string[5];
        sprintf(string, "%d", inode_index);
        char diskCommand[256];
        char indexString[20];
        int cylinder_index = physical_block_index / sector_per_cylinder;
        int sector_index = physical_block_index % sector_per_cylinder;
        strncat(diskCommand, "W", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        strncat(diskCommand, string, strlen(string));
        write(client_sockfd, diskCommand, strlen(diskCommand));
        return;
    }

    // Use single indirect link
    // First node in single indirect link
    else if(logical_block_index == 4){
        curDir->single_indirect_link = physical_block_index;
        int next_physical_block_index;
        int i;
        for(i=0; i<BLOCK_NUM; i++)
            if(emptyBlocks[i] == IS_EMPTY)
                break;
        next_physical_block_index = i;
        mySuperBlock.free_blocks_count--;
        emptyBlocks[next_physical_block_index] = IS_FULL;
        char string1[5];
        char string2[5];
        //itoa(inode_index, string1, 10);
        //itoa(next_physical_block_index, string2, 10);
        sprintf(string1, "%d", inode_index);
        sprintf(string2, "%d", next_physical_block_index);
        strcpy(blocks[curDir->single_indirect_link].data, string2);
        strncat(blocks[curDir->single_indirect_link].data, "@", 1);    // for seperation between different index
        strcpy(blocks[next_physical_block_index].data, string1);
        return;
    }
    // Not the first node but still in single indirect link
    else if(logical_block_index < 12){
        char string1[5];
        char string2[5];
        //itoa(physical_block_index, string1, 10);
        //itoa(inode_index, string2, 10);
        sprintf(string1, "%d", physical_block_index);
        sprintf(string2, "%d", inode_index);
        strncat(blocks[curDir->single_indirect_link].data, string1, strlen(string1));
        strncat(blocks[curDir->single_indirect_link].data, "@", 1);
        strcpy(blocks[physical_block_index].data, string2);
        return;
    }

    // Use double indirect link

    return;
}

/* The initialization of a directory */
void dirInit(Inode *curDir, int inode_index, int physical_block_index, int logical_block_index, char *request)
{
    // State update
    mySuperBlock.free_inodes_count--;
    mySuperBlock.free_blocks_count--;
    emptyInodes[inode_index] = IS_FULL;
    emptyBlocks[physical_block_index] = IS_FULL;
    curDir->next_available_block++;

    // New inode for directory created
    Inode *fileNode = &inodes[inode_index];
    fileNode->file_size = 0;
    fileNode->file_type = IS_DIR;
    //scanf("%s", fileNode->file_name);
    time(&(fileNode->creation_time));
    time(&(fileNode->last_modification_time));
    fileNode->next_available_block = 0;
    fileNode->last_directory_index = curDir->index;
    fileNode->index = inode_index;
    // Get the file name
    int i, startPoint, count = 0;
    char fileName[20];
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    for(i=startPoint; i<strlen(request); i++){
        fileName[count] = request[i];
        count++;
    }
    strcpy(fileNode->file_name, fileName);
    // debug
    printf("Filename: %s\n", fileName);

    // Create the connection between curDir and fileNode
    // Use direct block
    if(logical_block_index < 4){
        curDir->direct_block[logical_block_index] = physical_block_index;
        char string[5];
        sprintf(string, "%d", inode_index);
        char diskCommand[256];
        char indexString[20];
        int cylinder_index = physical_block_index / sector_per_cylinder;
        int sector_index = physical_block_index % sector_per_cylinder;
        strncat(diskCommand, "W", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        strncat(diskCommand, string, strlen(string));
        // debug
        printf("diskCommand: %s\n", diskCommand);
        write(client_sockfd, diskCommand, strlen(diskCommand));
        return;
    }

    // Use single indirect link
    // First node in single indirect link
    else if(logical_block_index == 4){
        curDir->single_indirect_link = physical_block_index;
        int next_physical_block_index;
        int i;
        for(i=0; i<BLOCK_NUM; i++)
            if(emptyBlocks[i] == IS_EMPTY)
                break;
        next_physical_block_index = i;
        mySuperBlock.free_blocks_count--;
        emptyBlocks[next_physical_block_index] = IS_FULL;
        char string1[5];
        char string2[5];
        //itoa(inode_index, string1, 10);
        //itoa(next_physical_block_index, string2, 10);
        sprintf(string1, "%d", inode_index);
        sprintf(string2, "%d", next_physical_block_index);
        strcpy(blocks[curDir->single_indirect_link].data, string2);
        strncat(blocks[curDir->single_indirect_link].data, "@", 1);    // for seperation between different index
        strcpy(blocks[next_physical_block_index].data, string1);
        return;
    }
    // Not the first node but still in single indirect link
    else if(logical_block_index < 12){
        char string1[5];
        char string2[5];
        //itoa(physical_block_index, string1, 10);
        //itoa(inode_index, string2, 10);
        sprintf(string1, "%d", physical_block_index);
        sprintf(string2, "%d", inode_index);
        strncat(blocks[curDir->single_indirect_link].data, string1, strlen(string1));
        strncat(blocks[curDir->single_indirect_link].data, "@", 1);
        strcpy(blocks[physical_block_index].data, string2);
        return;
    }

    // Use double indirect link

    return;
}

/* Look for the file in double_indirect_link in inode, return -1 if not found, return the block index which stores the inode index if found */
int double_indirect_search(Inode *curDir, char *name)
{
    return -1;
}

/* Look for the file in single_indirect_link in inode, return -1 if not found, return the block index which stores the inode index if found */
int single_indirect_search(Inode *curDir, char *name)
{
    Inode *destFile;
    int first_level_block_index;
    int second_level_block_index;
    int inode_index;
    first_level_block_index = curDir->single_indirect_link;
    char curIndexString[10];
    char char_to_load[2];
    char *all_index = blocks[first_level_block_index].data;
    for(int i=0; i<strlen(all_index); i++){
        // Meet seperator, check the current index
        if(all_index[i] == '@'){
            second_level_block_index = atoi(curIndexString);
            inode_index = atoi(blocks[second_level_block_index].data);
            destFile = &inodes[inode_index];
            if(strcmp(destFile->file_name, name) == 0)
                return second_level_block_index;
            curIndexString[0] = '\0';   // clear it
        }
        // Load the curIndexString
        else{
            char_to_load[0] = all_index[i];
            strncat(curIndexString, char_to_load, 1);
        }
    }
    // If the loop finishes, it means the file is not found
    return -1;
}

/* Look for the file in the direct_blocks in inode, return -1 if not found, return the block index which stores the inode index if found */
int direct_search(Inode *curDir, char *name)
{
    int block_index;
    int inode_index;
    Inode *destFile;
    int ceiling;

    char buffer[256];
    char diskCommand[256];
    int cylinder_index, sector_index;
    char indexString[20];

    if(curDir->next_available_block > 3)    ceiling = 4;
    else    ceiling = curDir->next_available_block;
    for(int i=0; i<ceiling; i++){
        block_index = curDir->direct_block[i];
        //inode_index = atoi(blocks[block_index].data);
        cylinder_index = block_index / sector_per_cylinder;
        sector_index = block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        printf("debug1\n");
        write(client_sockfd, diskCommand, strlen(diskCommand));
        printf("debug2\n");
        read(client_sockfd, buffer, 256);
        printf("debug3\n");
        inode_index = atoi(buffer);
        destFile = &inodes[inode_index];
        // Find the file
        if(strcmp(destFile->file_name, name) == 0)
            return block_index;
    }
    return -1;
}

/* Display the data of the file destFile */
void display_data(Inode *destFile)
{
    int direct_index;
    int first_level_index, second_level_index;
    int i;
    
    int ceiling;
    if(destFile->next_available_block > 3) ceiling = 4;
    else ceiling = destFile->next_available_block;
    // Display in direct blocks
    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char data[256];
    for(i=0; i<ceiling; i++){
        direct_index = destFile->direct_block[i];
        if(emptyBlocks[direct_index] == IS_FULL){
            cylinder_index = direct_index / sector_per_cylinder;
            sector_index = direct_index % sector_per_cylinder;
            strncat(diskCommand, "R", 2);
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", cylinder_index);
            strncat(diskCommand, indexString, strlen(indexString));
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", sector_index);
            strncat(diskCommand, indexString, strlen(indexString));
            write(client_sockfd, diskCommand, strlen(diskCommand));
            read(client_sockfd, data, 256);
            write(newsockfd, data, 256);
        }
    }

    if(destFile->next_available_block <= 4){
        //write_log("\n");
        write(newsockfd, "\n", 2);
        return;
    }

    // Display in single indirect link
    first_level_index = destFile->single_indirect_link;
    char *all_index = blocks[first_level_index].data;
    char curIndexString[10];
    char char_to_load[2];
    for(i=0; i<strlen(all_index); i++){
        // Meet seperator, check the current index
        if(all_index[i] == '@'){
            second_level_index = atoi(curIndexString);
            if(emptyBlocks[second_level_index] == IS_FULL)
                write_log(blocks[second_level_index].data);
            curIndexString[0] = '\0';
        }
        else{
            char_to_load[0] = all_index[i];
            strncat(curIndexString, char_to_load, 1);
        }
    }

    // Display in double indirect link

    write_log("\n");
    return;
}

/* Overwrite data with length of dataLen in destFile */
void overwrite(Inode *destFile, int dataLen, char *data)
{
    int i;
    int block_index, second_block_index;
    // Clear direct blocks
    /*for(i=0; i<4; i++){
        block_index = destFile->direct_block[i];
        emptyBlocks[block_index] = IS_EMPTY;
    }*/
    // Clear single indirect link
    char curIndexString[10];
    char all_index[256];
    char char_to_load[2];
    block_index = destFile->single_indirect_link;
    //all_index = blocks[block_index].data;
    int cylinder_index = block_index / sector_per_cylinder;
    int sector_index = block_index % sector_per_cylinder;
    char diskCommand[256];
    char indexString[20];
    char buffer[256];
    strncat(diskCommand, "R", 2);
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", cylinder_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    write(client_sockfd, diskCommand, strlen(diskCommand));
    read(client_sockfd, buffer, 256);
    strcpy(all_index, buffer);

    for(i=0; i<strlen(all_index); i++){
        if(all_index[i] == '@'){
            second_block_index = atoi(curIndexString);
            emptyBlocks[second_block_index] = IS_EMPTY;
            curIndexString[0] = '\0';
        }
        else{
            char_to_load[0] = all_index[i];
            strncat(curIndexString, char_to_load, 1);
        }
    }

    // Clear double indirect link

    int block_required = dataLen / BLOCK_SIZE + 1;  // these lines' problem
    int tmp;
    for(i=0; i<BLOCK_NUM; i++){
        if(emptyBlocks[i] == IS_EMPTY){
            tmp = i;
            break;
        }
    }
    destFile->direct_block[0] = tmp;
    block_index = destFile->direct_block[0];
    //strcpy(blocks[block_index].data, data);
    cylinder_index = block_index / sector_per_cylinder;
    sector_index = block_index % sector_per_cylinder;
    diskCommand[0] = '\0';
    strncat(diskCommand, "W", 2);
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", cylinder_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    strncat(diskCommand, data, strlen(data));
    write(client_sockfd, diskCommand, strlen(diskCommand));

    emptyBlocks[block_index] = IS_FULL;
    destFile->next_available_block = 1;
    destFile->file_size = strlen(data);
    time(&(destFile->last_modification_time));
    return;
}

/* Insert to destFile at the position before the pos-th character(0-indexed), with the dataLen bytes of data */
void insert_data(Inode *destFile, int pos, int dataLen, char *data)
{
    time(&(destFile->last_modification_time));
    destFile->file_size += strlen(data);

    int i;
    int block_index;
    block_index = destFile->direct_block[0];
    //char *data_to_modify = blocks[block_index].data;
    char data_to_modify[256];
    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];
    cylinder_index = block_index / sector_per_cylinder;
    sector_index = block_index % sector_per_cylinder;
    strncat(diskCommand, "R", 2);
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", cylinder_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    write(client_sockfd, diskCommand, 256);
    read(client_sockfd, buffer, 256);
    strcpy(data_to_modify, buffer);

    char mybuffer[BLOCK_SIZE];
    int count = 0;
    for(i=pos; i<strlen(data_to_modify); i++){
        mybuffer[count] = data_to_modify[count + pos];
        count ++;
    }

    data_to_modify[pos] = '\0';
    strncat(data_to_modify, data, dataLen); // insert new data
    strncat(data_to_modify, mybuffer, count); // append original data

    return;
}

/* Delete the contents in destFile from the pos character(0-indexed), delete l bytes or till the end of the file*/
void delete_data(Inode *destFile, int pos, int dataLen)
{
    time(&(destFile->last_modification_time));
    destFile->file_size -= dataLen;

    int i;
    int block_index;
    block_index = destFile->direct_block[0];
    //char *data_to_modify = blocks[block_index].data;
    char data_to_modify[256];
    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];
    cylinder_index = block_index / sector_per_cylinder;
    sector_index = block_index % sector_per_cylinder;
    strncat(diskCommand, "R", 2);
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", cylinder_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    write(client_sockfd, diskCommand, 256);
    read(client_sockfd, buffer, 256);
    strcpy(data_to_modify, buffer);

    char mybuffer[BLOCK_SIZE];
    int count = 0;
    for(i=pos; i<strlen(data_to_modify); i++){
        mybuffer[count] = data_to_modify[count + i];
        count ++;
    }
    data_to_modify[pos] = '\0';
    strncat(data_to_modify, mybuffer, count);
    return;
}

/* Shift left every character in data by one spot */
void left_shift(char *data)
{
    int i;
    int len = strlen(data);
    for(i=0; i<len-1; i++)
        data[i] = data[i+1];
    data[len-1] = '\0';
    return;
}

/* Format the file system on the disk */
// > f
void format_system(int sock)
{
    mtx.lock();
    superBlockInit();
    homeDirInit();
    mtx.unlock();s
    write(sock, "Done\n", 10);
    return;
}

/* Create a file named f in the file system */
// > mk f
void create_file(int sock, char *request)
{
    int cylinder_index, sector_index;

    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    int inode_index;
    int physical_block_index;
    int logical_block_index = curDir->next_available_block;
    int i;

    rcu_read_lock();
    // get inode_index
    for(i=0; i<INODE_NUM; i++)
        if(emptyInodes[i] == IS_EMPTY)
            break;
    if(i == INODE_NUM){
        //write_log("No\n");  // no available inode
        write(newsockfd, "No\n", 10);
        return;
    }
    else inode_index = i;

    // get physical_block_index
    for(i=0; i<BLOCK_NUM; i++)
        if(emptyBlocks[i] == IS_EMPTY)
            break;
    if(i == BLOCK_NUM){
        //write_log("No\n");  // no available block
        write(newsockfd, "No\n", 10);
        return;
    }
    else physical_block_index = i;
    rcu_read_unlock();

    fileInit(curDir, inode_index, physical_block_index, logical_block_index, request);

    cylinder_index = physical_block_index / sector_per_cylinder;
    sector_index = physical_block_index % sector_per_cylinder;
    /*char diskCommand[256];
    char indexString[10];
    strncat(diskCommand, "W ", 3);
    sprintf(indexString, "%d", cylinder_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", inode_index);
    strncat(diskCommand, indexString, strlen(indexString));

    write(client_sockfd, diskCommand, strlen(diskCommand));*/

    //write_log("Yes\n");
    write(newsockfd, "Yes\n", 10);
    return;
}

/* Create a subdirectory named d in the currect directory */
// > mkdir d
void create_directory(int sock, char *request)
{
    int cylinder_index, sector_index;

    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    int inode_index;
    int physical_block_index;
    int logical_block_index = curDir->next_available_block;
    int i;

    rcu_read_lock();
    // get inode_index
    for(i=0; i<INODE_NUM; i++)
        if(emptyInodes[i] == IS_EMPTY)
            break;
    if(i == INODE_NUM){
        //write_log("No\n");
        write(newsockfd, "No\n", 10);
        return;
    }
    else inode_index = i;

    // get physical_block_index
    for(i=0; i<BLOCK_NUM; i++)
        if(emptyBlocks[i] == IS_EMPTY)
            break;
    if(i == BLOCK_NUM){
        //write_log("No\n");
        write(newsockfd, "No\n", 10);
        return;
    }
    else physical_block_index = i;
    rcu_read_unlock();

    dirInit(curDir, inode_index, physical_block_index, logical_block_index, request);

    /*cylinder_index = physical_block_index / sector_per_cylinder;
    sector_index = physical_block_index % sector_per_cylinder;
    char diskCommand[256];
    char indexString[10];
    strncat(diskCommand, "W ", 3);
    sprintf(indexString, "%d", cylinder_index); 
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", sector_index);
    strncat(diskCommand, indexString, strlen(indexString));
    strncat(diskCommand, " ", 2);
    sprintf(indexString, "%d", inode_index);
    strncat(diskCommand, indexString, strlen(indexString));

    write(client_sockfd, diskCommand, strlen(diskCommand));*/

    //write_log("Yes\n");
    write(sock, "Yes\n", 10);
    return;
}

/* Delete the file named f from current directory*/
// > rm f
void delete_file(int sock, char *request)
{
    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    char name[20];
    int file_inode_index = -1;  // The inode of file
    int file_block_index = -1;

    //scanf("%s", name);
    // Get the file name
    int i, startPoint, count = 0;
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    for(i=startPoint; i<strlen(request); i++){
        name[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        name[i] = '\0';

    // Double indirect link has files
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, name);
        // Find the file, then delete it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            mtx.lock();
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            mySuperBlock.free_inodes_count--;
            mySuperBlock.free_blocks_count--;
            emptyInodes[file_inode_index] = IS_EMPTY;
            emptyInodes[file_block_index] = IS_EMPTY;
            mtx.unlock();
            return;
        }
        // Otherwise, continue
    }

    // Single indirect link has files
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, name);
        // Find the file, then delete it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            mtx.lock();
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            mySuperBlock.free_inodes_count--;
            mySuperBlock.free_blocks_count--;
            emptyInodes[file_inode_index] = IS_EMPTY;
            emptyInodes[file_block_index] = IS_EMPTY;
            mtx.unlock();
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has files
    file_block_index = direct_search(curDir, name);
    // Find the file, then delete it and return
    if(file_block_index != -1){
        mtx.lock();
        //write_log("Yes\n");
        write(newsockfd, "Yes\n", 10);
        //file_inode_index = atoi(blocks[file_block_index].data);
        int cylinder_index, sector_index;
        char indexString[20];
        char diskCommand[256];
        char buffer[256];
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        int direct_index, i, ceiling;
        int index1, index2;
        if(curDir->next_available_block > 3)    ceiling = 4;
        else    ceiling = curDir->next_available_block;

        for(i=0; i<ceiling; i++){
            if(curDir->direct_block[i] == file_block_index){
                direct_index = i;
                break;
            }
        }
        for(i=direct_index; i<ceiling-1; i++){
            index1 = curDir->direct_block[i];
            index2 = curDir->direct_block[i+1];
            //strcpy(blocks[index1].data, blocks[index2].data);
            // First get data of index2 into buffer
            cylinder_index = index2 / sector_per_cylinder;
            sector_index = index2 % sector_per_cylinder;
            strncat(diskCommand, "R", 2);
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", cylinder_index);
            strncat(diskCommand, indexString, strlen(indexString));
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", sector_index);
            strncat(diskCommand, indexString, strlen(indexString));
            write(client_sockfd, diskCommand, 256);
            read(client_sockfd, buffer, 256);
            // Then write the data into index1
            cylinder_index = index1 / sector_per_cylinder;
            sector_index = index1 % sector_per_cylinder;
            strncat(diskCommand, "W", 2);
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", cylinder_index);
            strncat(diskCommand, indexString, strlen(indexString));
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", sector_index);
            strncat(diskCommand, indexString, strlen(indexString));
            strncat(diskCommand, " ", 2);
            strncat(diskCommand, buffer, strlen(buffer));
            write(client_sockfd, diskCommand, 256);
        }

        curDir->next_available_block--;
        mySuperBlock.free_inodes_count--;
        mySuperBlock.free_blocks_count--;
        emptyInodes[file_inode_index] = IS_EMPTY;
        emptyInodes[file_block_index] = IS_EMPTY;
        mtx.unlock();
        return;
    }
    // Otherwise, file doesn't exist in this directory
    //write_log("No\n");
    write(sock, "No\n", 10);
    return;
}

/* Change the current working directory to the path */
// > cd path
void change_directory(int sock, char *request)
{
    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    char dest[20];
    int i, startPoint, count = 0;
    rcu_read_lock();
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    for(i=startPoint; i<strlen(request); i++){
        dest[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        dest[count] = '\0';
    rcu_read_unlock();
    
    // To current directory, do nothing
    if(strcmp(dest, ".") == 0){
        write(sock, "Yes\n", 5);
        return;
    } 
    
    // To last directory
    else if(strcmp(dest, "..") == 0){
        mySuperBlock.curDirIndex = curDir->last_directory_index;
        write(sock, "Yes\n", 5);
        return;
    }

    // Otherwise, it is the file in the directory
    int file_inode_index = -1;
    int file_block_index = -1;
    // Double indirect link has files
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, dest);
        // Find the file, then go to it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            mySuperBlock.curDirIndex = file_inode_index;
            return;
        }
        // Otherwise, continue
    }

    // Single indirect link has files
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, dest);
        // Find the file, then go to it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            mySuperBlock.curDirIndex = file_inode_index;
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has files
    file_block_index = direct_search(curDir, dest);
    // Find the filel, then go to it and return
    if(file_block_index != -1){
        //write_log("Yes\n");
        write(newsockfd, "Yes\n", 10);
        //file_inode_index = atoi(blocks[file_block_index].data);
        int cylinder_index, sector_index;
        char indexString[20];
        char diskCommand[256];
        char buffer[256];
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        mySuperBlock.curDirIndex = file_inode_index;
        return;
    }
    // Otherwise, directory doesn't exist in this directory
    //write_log("No\n");
    write(newsockfd, "No\n", 10);
    return;
}

/* Delete the directory named d in the current directory */
// > rmdir d
void delete_directory(char *request)
{
    delete_file(request);
    return;
}

/* Return a listing of the files and directories in the current directory */
// > ls
void directory_listing()
{
    // Here I choose to use the form of <fileName> to represent directories
    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    Inode *listDir;
    int block_index, second_level_block_index, inode_index;
    int i;
    char outputName[256];

    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];
    char file_size_string[20];
    
    // List possible files in direct_block
    int direct_blocks_to_index;
    if(curDir->next_available_block > 3) direct_blocks_to_index = 4;
    else direct_blocks_to_index = curDir->next_available_block;
    for(i=0; i<direct_blocks_to_index; i++){
        block_index = curDir->direct_block[i];
        if(emptyBlocks[block_index] == IS_FULL){
            //inode_index = atoi(blocks[block_index].data);
            cylinder_index = block_index / sector_per_cylinder;
            sector_index = block_index % sector_per_cylinder;
            strncat(diskCommand, "R", 2);
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", cylinder_index);
            strncat(diskCommand, indexString, strlen(indexString));
            strncat(diskCommand, " ", 2);
            sprintf(indexString, "%d", sector_index);
            strncat(diskCommand, indexString, strlen(indexString));
            write(client_sockfd, diskCommand, 256);
            read(client_sockfd, buffer, 256);
            inode_index = atoi(buffer);

            listDir = &inodes[inode_index];
            // After outputing all the files, output "last" to denote the end
            // Use "<>" to denote directory
            if(listDir->file_type == IS_DIR){
                /*write_log("<");
                write_log(listDir->file_name);
                write_log(">");
                write_log("\t");
                write_log(listDir->file_size);
                write_log("\t");
                write_log(ctime(&(listDir->creation_time)));
                write_log("\t");
                write_log(ctime(&(listDir->last_modification_time)));
                write_log("\n");*/
                strncat(outputName, "<", 2);
                strncat(outputName, listDir->file_name, strlen(listDir->file_name));
                strncat(outputName, ">", 2);
                strncat(outputName, "\t", 2);
                sprintf(file_size_string, "%d", listDir->file_size);
                strncat(outputName, file_size_string, strlen(file_size_string));
                strncat(outputName, "\t", 2);
                strncat(outputName, ctime(&(listDir->creation_time)), strlen(ctime(&(listDir->creation_time))));
                strncat(outputName, "\t", 2);
                strncat(outputName, ctime(&(listDir->last_modification_time)), strlen(ctime(&(listDir->last_modification_time))));
                strncat(outputName, "\n", 2);
                write(newsockfd, outputName, strlen(outputName));
            }
            // Use "[]" to denote file
            else{
                /*write_log("[");
                write_log(listDir->file_name);
                write_log("]");
                write_log("\t");
                write_log(listDir->file_size);
                write_log("\t");
                write_log(ctime(&(listDir->creation_time)));
                write_log("\t");
                write_log(ctime(&(listDir->last_modification_time)));
                write_log("\n");*/
                strncat(outputName, "[", 2);
                strncat(outputName, listDir->file_name, strlen(listDir->file_name));
                strncat(outputName, "]", 2);
                strncat(outputName, "\t", 2);
                sprintf(file_size_string, "%d", listDir->file_size);
                strncat(outputName, file_size_string, strlen(file_size_string));
                strncat(outputName, "\t", 2);
                strncat(outputName, ctime(&(listDir->creation_time)), strlen(ctime(&(listDir->creation_time))));
                strncat(outputName, "\t", 2);
                strncat(outputName, ctime(&(listDir->last_modification_time)), strlen(ctime(&(listDir->last_modification_time))));
                strncat(outputName, "\n", 2);
                write(newsockfd, outputName, strlen(outputName));
            }
        }
    }

    if(curDir->next_available_block <= 4){
        //write_log("\n");
        write(newsockfd, "last", 10);
        return;
    }

    // Listing possible files in single indirect link
    block_index = curDir->single_indirect_link;
    if(emptyBlocks[block_index]==IS_FULL){
        // Examine all elements in single_indirect_link
        char curIndexString[10];
        //char *all_index = blocks[block_index].data;
        char all_index[256];
        cylinder_index = block_index / sector_per_cylinder;
        sector_index = block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        strcpy(all_index, buffer);

        char char_to_load[2];
        for(i=0; i<strlen(all_index); i++){
            if(all_index[i] == '@'){
                second_level_block_index = atoi(curIndexString);
                //inode_index = atoi(blocks[second_level_block_index].data);
                cylinder_index = second_level_block_index / sector_per_cylinder;
                sector_index = second_level_block_index % sector_per_cylinder;
                strncat(diskCommand, "R", 2);
                strncat(diskCommand, " ", 2);
                sprintf(indexString, "%d", cylinder_index);
                strncat(diskCommand, indexString, strlen(indexString));
                strncat(diskCommand, " ", 2);
                sprintf(indexString, "%d", sector_index);
                strncat(diskCommand, indexString, strlen(indexString));
                write(client_sockfd, diskCommand, 256);
                read(client_sockfd, buffer, 256);
                inode_index = atoi(buffer);

                listDir = &inodes[inode_index];
                if(listDir->file_type == IS_DIR){
                    write_log("<");
                    write_log(listDir->file_name);
                    write_log(">");
                    write_log("  ");
                }
                else{
                    write_log(listDir->file_name);
                    write_log("  ");
                }
                curIndexString[0] = '\0';
            }
            else{
                char_to_load[0] = all_index[i];
                strncat(curIndexString, char_to_load, 1);
            }
        }
    }

    // Listing possible files in double indirect link

    //write_log("\n");
    write(newsockfd, "\n", 2);
    return;
}

/* Read the file named f, and return the data that came from it */
// > cat f
void catch_file(char *request)
{
    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    Inode *destFile;
    char name[20];
    int file_inode_index = -1;  // The inode of file
    int file_block_index = -1;

    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];

    //scanf("%s", name);
    // Get the name
    int i, startPoint, count = 0;
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    for(i=startPoint; i<strlen(request); i++){
        name[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        name[i] = '\0';

    // Double indirect link has the file
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, name);
        // Find the file, then display it and return
        if(file_block_index != -1){
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            display_data(destFile);
            return;
        }
        // Otherwise, continue
    }

    // Single indirect link has the file
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, name);
        // Find the file, then display it and return
        if(file_block_index != -1){
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            display_data(destFile);
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has the file
    file_block_index = direct_search(curDir, name);
    // Find the file, then display it and return
    if(file_block_index != -1){
        //file_inode_index = atoi(blocks[file_block_index].data);
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        destFile = &inodes[file_inode_index];
        display_data(destFile);
        return;
    }
    // Otherwise, file doesn't exist in this directory
    //write_log("No\n");
    write(newsockfd, "No\n", 10);
    return;
}

/* Overwrite the contents of the file named f with the l bytes of data */
// > w f l data
void write_file(char *request)
{
    char fileName[20];
    int dataLen;
    char data[BLOCK_SIZE];
    //scanf("%s%d", fileName, &dataLen);
    //scanf("%[^\n]", data);  // this will read one more blank
    // Get the fileName and dataLen and data
    int i, startPoint, count = 0;
    char dataLen_string[20];
    // Eat W and blank
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    // Read fileName
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        fileName[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        fileName[i] = '\0';
    count = 0;
    // Read dataLen
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        dataLen_string[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        dataLen_string[i] = '\0';
    count = 0;
    dataLen = atoi(dataLen_string);
    // Read data
    for(i=startPoint; i<strlen(request); i++){
        data[count] = request[i];
        count++;
    }
    for(i=count; i<BLOCK_SIZE; i++)
        data[i] = '\0';

    // Dealing with extra blank
    left_shift(data);

    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    Inode *destFile;
    int file_inode_index = -1;
    int file_block_index = -1;

    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];

    // Double indirect link has the file
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            overwrite(destFile, dataLen, data);
            return;
        }
    }

    // Single indirect link has the file
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            overwrite(destFile, dataLen, data);
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has the file
    file_block_index = direct_search(curDir, fileName);
    // Find the file, then overwrite it and return
    if(file_block_index != -1){
        //write_log("Yes\n");
        write(newsockfd, "Yes\n", 10);
        //file_inode_index = atoi(blocks[file_block_index].data);
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        destFile = &inodes[file_inode_index];
        overwrite(destFile, dataLen, data); // just its problem
        return;
    }
    // Otherwise, file doesn't exist in this directory
    //write_log("No\n");
    write(newsockfd, "No\n", 10);
    return;
}

/* Insert to the file at the position before the pos-th character(0-indexed), with the l bytes of data */
// > i f pos l data
void insert_to_file(char *request)
{
    char fileName[20];
    int pos;
    int dataLen;
    char data[BLOCK_SIZE];
    //scanf("%s%d%d", fileName, &pos, &dataLen);
    //scanf("%[^\n]", data);
    // Get the fileName and dataLen and data
    int i, startPoint, count = 0;
    char pos_string[20];
    char dataLen_string[20];
    // Eat i and blank
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    // Read fileName
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        fileName[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        fileName[i] = '\0';
    count = 0;
    // Read pos
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        pos_string[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        pos_string[i] = '\0';
    count = 0;
    pos = atoi(pos_string);
    // Read dataLen
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        dataLen_string[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        dataLen_string[i] = '\0';
    count = 0;
    dataLen = atoi(dataLen_string);
    // Read data
    for(i=startPoint; i<strlen(request); i++){
        data[count] = request[i];
        count++;
    }
    for(i=count; i<BLOCK_SIZE; i++)
        data[i] = '\0';

    left_shift(data);

    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    Inode *destFile;
    int file_inode_index = -1;
    int file_block_index = -1;

    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];

    // Double indirect link has the file
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            insert_data(destFile, pos, dataLen, data);
            return;
        }
    }

    // Single indirect link has the file
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            insert_data(destFile, pos, dataLen, data);
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has the file
    file_block_index = direct_search(curDir, fileName);
    // Find the file, then overwrite it and return
    if(file_block_index != -1){
        //write_log("Yes\n");
        write(newsockfd, "Yes\n", 10);
        //file_inode_index = atoi(blocks[file_block_index].data);
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        destFile = &inodes[file_inode_index];
        insert_data(destFile, pos, dataLen, data);
        return;
    }
    // Otherwise, file doesn't exist in this directory
    //write_log("No\n");
    write(newsockfd, "No\n", 10);
    return;
}

/* Delete the contents from the pos character(0-indexed), delete l bytes or till the end of the file */
// > d f pos l
void delete_in_file(char *request)
{
    char fileName[20];
    int pos;
    int dataLen;
    //scanf("%s%d%d", fileName, &pos, &dataLen);
    // Get the fileName and dataLen and data
    int i, startPoint, count = 0;
    char pos_string[20];
    char dataLen_string[20];
    // Eat d and blank
    for(i=0; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
    }
    // Read fileName
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        fileName[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        fileName[i] = '\0';
    count = 0;
    // Read pos
    for(i=startPoint; i<strlen(request); i++){
        if(request[i] == ' '){
            startPoint = i + 1;
            break;
        }
        pos_string[count] = request[i];
        count++;
    }
    for(i=count; i<20; i++)
        pos_string[i] = '\0';
    count = 0;
    pos = atoi(pos_string);
    // Read dataLen
    for(i=startPoint; i<strlen(request); i++){
        dataLen_string[count] = request[i];
        count++;
    }
    for(i=count; i<BLOCK_SIZE; i++)
        dataLen_string[i] = '\0';
    dataLen = atoi(dataLen_string);

    Inode *curDir = &inodes[mySuperBlock.curDirIndex];
    Inode *destFile;
    int file_inode_index = -1;
    int file_block_index = -1;

    int cylinder_index, sector_index;
    char indexString[20];
    char diskCommand[256];
    char buffer[256];

    // Double indirect link has the file
    if(curDir->next_available_block > 12){
        file_block_index = double_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            delete_data(destFile, pos, dataLen);
            return;
        }
    }

    // Single indirect link has the file
    if(curDir->next_available_block > 4){
        file_block_index = single_indirect_search(curDir, fileName);
        // Find the file, then overwrite it and return
        if(file_block_index != -1){
            //write_log("Yes\n");
            write(newsockfd, "Yes\n", 10);
            file_inode_index = atoi(blocks[file_block_index].data);
            destFile = &inodes[file_inode_index];
            delete_data(destFile, pos, dataLen);
            return;
        }
        // Otherwise, continue
    }

    // Only direct blocks has the file
    file_block_index = direct_search(curDir, fileName);
    // Find the file, then overwrite it and return
    if(file_block_index != -1){
        //write_log("Yes\n");
        write(newsockfd, "Yes\n", 10);
        //file_inode_index = atoi(blocks[file_block_index].data);
        cylinder_index = file_block_index / sector_per_cylinder;
        sector_index = file_block_index % sector_per_cylinder;
        strncat(diskCommand, "R", 2);
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", cylinder_index);
        strncat(diskCommand, indexString, strlen(indexString));
        strncat(diskCommand, " ", 2);
        sprintf(indexString, "%d", sector_index);
        strncat(diskCommand, indexString, strlen(indexString));
        write(client_sockfd, diskCommand, 256);
        read(client_sockfd, buffer, 256);
        file_inode_index = atoi(buffer);

        destFile = &inodes[file_inode_index];
        delete_data(destFile, pos, dataLen);
        return;
    }
    // Otherwise, file doesn't exist in this directory
    //write_log("No\n");
    write(newsockfd, "No\n", 10);
    return;
}

/* Doing the work of server */
int serverWork()
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
    printf("Successfully accept the connection from client!\n");
	return newsockfd;	// never get here
}

/* Doing the work of client */
void clientWork()
{
    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)host->h_addr, (char *)&serv_addr.sin_addr.s_addr, host->h_length);
    serv_addr.sin_port = htons(serverPort);
    if(connect(client_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("Error connecting!\n");
    return;   // never get here
}