#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>


#include "../header/commonfuncs.h"

#define print(x) write(1, x, strlen(x));

#define ERR_SOCKET "Error: socket not created"
#define ERR_CONNECT "Error: connection not established"

#define ERR -1

typedef struct{
	char* server_name; 
	char* directory;
	char* discovery_ip;
	int discovery_port;
    char* poole_ip;
	int poole_port;
} Poole;

Poole poole;

void free_everything(){
    free(poole.server_name);
    free(poole.directory);
    free(poole.discovery_ip);
    free(poole.poole_ip);
}

int main(int argc, char *argv[]){
	
    char* string;
	const char* file_path_start = "config/";
    char* file_path = NULL;
    int server_fd = -1;
    struct sockaddr_in serv_addr;

    if (argc != 2){
        print("Error: There must be exactly one parameter when running\n");
        return 0;
    }

    file_path = (char*) malloc(sizeof(char) * (strlen(file_path_start) + strlen(argv[1]) - 1));
    strcat(file_path, file_path_start);
    strcat(file_path, argv[1]);

    int fd_config = open(file_path, O_RDONLY);
    if (fd_config == -1){
        perror("Error: unable to open file\n");
        exit(EXIT_FAILURE);
    }

	poole.server_name = read_until(fd_config, '\n');
	poole.directory = read_until(fd_config, '\n');
	poole.discovery_ip = read_until(fd_config, '\n');
	string = read_until(fd_config, '\n');
	poole.discovery_port = atoi(string);
    free(string);
    poole.poole_ip = read_until(fd_config, '\n');
    string = read_until(fd_config, '\n');
	poole.poole_port = atoi(string);
    free(string);
    close(fd_config);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    serv_addr.sin_addr.s_addr = inet_addr(poole.discovery_ip);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(poole.discovery_port);

    if (connect(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logn(ERR_CONNECT);
        return ERR;
    }

    

    free_everything();
	
    return 0;
}