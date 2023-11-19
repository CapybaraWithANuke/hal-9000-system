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
#define ERR_BIND "Error: binding failed"
#define ERR_LISTEN "Error: listening failed"
#define ERR_ACCEPT "Error: accepting failed"

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

int setupSocket(char* ip, int port, int server_fd) {

    struct sockaddr_in address;
    socklen_t addrlen;
    int socket_fd = -1;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);
    addrlen = sizeof(address);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        logn(ERR_BIND);
        return ERR;
    }

    if (listen(server_fd, 5) < 0) {
        logn(ERR_LISTEN);
        return ERR;
    }

    if ((socket_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
        logn(ERR_ACCEPT);
        return ERR;
    }

    return socket_fd;

}

int main(int argc, char *argv[]){
	
    char* string;
	const char* file_path_start = "config/";
    char* file_path = NULL;
    int discovery_fd = -1;
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





    discovery_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (discovery_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    serv_addr.sin_addr.s_addr = inet_addr(poole.discovery_ip);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(poole.discovery_port);

    if (connect(discovery_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logn(ERR_CONNECT);
        return ERR;
    }

    char* frame_data = calloc(1, sizeof(char));
    strcat(frame_data, poole.server_name);
    strcat(frame_data, '&');
    strcat(frame_data, poole.poole_ip);
    strcat(frame_data, '&');
    strcat(frame_data, poole.poole_port);
    

    char* frame = buildFrame(1, strlen("NEW_POOLE"), "NEW_POOLE", frame_data);

    free(frame_data);

    write(discovery_fd, frame, 256);
    Packet packet = read_packet(discovery_fd);

    while (packet.header[4] == 'K') {
        char* frame_data = calloc(1, sizeof(char));
        strcat(frame_data, poole.server_name);
        strcat(frame_data, '&');
        strcat(frame_data, poole.poole_ip);
        strcat(frame_data, '&');
        strcat(frame_data, poole.poole_port);
        

        char* frame = buildFrame(1, strlen("NEW_POOLE"), "NEW_POOLE", frame_data);

        free(frame_data);

        write(discovery_fd, frame, 256);
        Packet packet = read_packet(discovery_fd);
    }

    close(discovery_fd);





    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    int socket_fd = setupSocket(poole.poole_ip, poole.poole_port, server_fd);

    
    


    free_everything();
	
    return 0;
}