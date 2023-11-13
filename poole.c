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

#include "commonfuncs.h"

#define print(x) write(1, x, strlen(x));

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

    const char* file_path_start = "config_files/";
    char* file_path = NULL;

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

    free_everything();
	
    return 0;
}