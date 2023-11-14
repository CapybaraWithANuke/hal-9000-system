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
#include <arpa/inet.h>

#include "../header/commonfuncs.h"

#define ERR_ARGC "There should be exactly 1 argument."
#define ERR_FD "Configuration file not found"

#define ERR_SOCKET "Socket not created"
#define ERR_BIND "Binding failed"
#define ERR_LISTEN "Listening failed"
#define ERR_ACCEPT "Accepting failed"

#define ERR -1

typedef struct {
    char* ip_poole;
    int port_poole;
    char* ip_bowman;
    int port_bowman;
} Config;

int server_poole_fd;
int server_bowman_fd;

int setupSocket(char* ip, int port, int server_fd) {

    struct sockaddr_in address;
    socklen_t addrlen;
    int socket_fd = -1; 

    logn("aboba");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    logn("bebra");
    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

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

int main(int argc, char** argv) {

    Config config;
    int fd;
	const char* file_path_start = "config/";
    char* file_path = NULL;
    char* buffer = NULL;
    int socket_poole_fd = -1;
    int socket_bowman_fd = -1;

    if (argc != 2) {
        logn(ERR_ARGC);
        return ERR;
    }

    file_path = (char*) malloc(sizeof(char) * (strlen(file_path_start) + strlen(argv[1]) - 1));
    strcat(file_path, file_path_start);
    strcat(file_path, argv[1]);

    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        logn(ERR_FD);
        return ERR;
    }

    config.ip_poole = read_until(fd, '\n');

    buffer = read_until(fd, '\n');
    config.port_poole = atoi(buffer);
    free(buffer);
    config.ip_bowman = read_until(fd, '\n');
    buffer = read_until(fd, '\n');
    config.port_bowman = atoi(buffer);

    socket_poole_fd = setupSocket(config.ip_poole, config.port_poole, server_poole_fd);
    socket_bowman_fd = setupSocket(config.ip_bowman, config.port_bowman, server_bowman_fd);

    if (socket_poole_fd == -1 || socket_bowman_fd == -1) {
        return ERR;
    }


    free(config.ip_poole);
    free(config.ip_bowman);
    free(file_path);
    free(buffer);
    close(fd);
    close(socket_bowman_fd); close(socket_poole_fd);
    close(server_poole_fd); close(server_bowman_fd);
    return 0;

}