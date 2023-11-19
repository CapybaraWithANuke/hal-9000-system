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

#define ERR_ARGC "Error: there should be exactly 1 argument."
#define ERR_FD "Error: configuration file not found"

#define ERR_SOCKET "Error: socket not created"
#define ERR_BIND "Error: binding failed"
#define ERR_LISTEN "Error: listening failed"
#define ERR_ACCEPT "Error: accepting failed"

#define ERR -1

#define print(x) write(1, x, strlen(x))

typedef struct {
    char* ip_poole;
    int port_poole;
    char* ip_bowman;
    int port_bowman;
} Config;

typedef struct {
    char* username;
    char* ip;
    char* port;
    int num_clients;
} Poole;

int* poole_fds;
int* bowman_fds;
Poole* poole_connections;

int setupSocket (char* ip, int port, int server_fd) {

    struct sockaddr_in address;
    socklen_t addrlen;
    int socket_fd = -1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
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

void poole_request(int fd,int pos){

    Packet packet = read_packet(fd);

    poole_connections[pos].num_clients = 0;

    char** strings = split(packet.data, '&');
    poole_connections[pos].username = strings[0];
    poole_connections[pos].ip = strings[1];
    poole_connections[pos].port = strings[2];
}

int process_requests (int* fds, int num_fds, fd_set set, int bowman_npoole) {

    int i, aux;

    for (i=0; i<num_fds; i++) {

        if (FD_ISSET(fds[i], &set)) {

            if (i == 0){
                if ((aux = accept(fds[0], NULL, 0)) > 0) {

                    debug("received poole connection\n");
                    fds = (int*) realloc(fds, sizeof(int)*(num_fds + 1));
                    fds[num_fds] = aux;
                    FD_SET(fds[num_fds], &set);
                    num_fds++;

                    if (!bowman_npoole) {
                        poole_connections = (Poole*) realloc (poole_connections, sizeof(Poole)*(num_fds));
                    }
                }
                else if (bowman_npoole) {
                    print("ERROR accepting bowman client connection.\n");
                }
                else {
                    print("ERROR accepting poole server connection.\n");
                }
            }
            else {
                if (!bowman_npoole){
                    poole_request(fds[i], i);
                    debug("Received poole request\n");
                }
                else {
                    //bowman_request(fds[i], num_fds, set);
                }
            }
        }
    }

    return num_fds;
}

int main (int argc, char** argv) {

    Config config;
    int file_fd;
	const char* file_path_start = "config/";
    char* file_path = NULL;
    char* buffer = NULL;
    fd_set poole_set;
    fd_set bowman_set;
    int num_poole_fds = 1, num_bowman_fds = 1;

    if (argc != 2) {
        logn(ERR_ARGC);
        return ERR;
    }

    file_path = (char*) malloc(sizeof(char) * (strlen(file_path_start) + strlen(argv[1]) - 1));
    strcat(file_path, file_path_start);
    strcat(file_path, argv[1]);

    file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        logn(ERR_FD);
        return ERR;
    }

    config.ip_poole = read_until(file_fd, '\n');
    config.ip_poole = read_until(file_fd, '\n');

    buffer = read_until(file_fd, '\n');
    buffer = read_until(file_fd, '\n');
    config.port_poole = atoi(buffer);
    free(buffer);
    config.ip_bowman = read_until(file_fd, '\n');
    buffer = read_until(file_fd, '\n');
    config.ip_bowman = read_until(file_fd, '\n');
    buffer = read_until(file_fd, '\n');
    config.port_bowman = atoi(buffer);
    close(file_fd);
    free(file_path);

    poole_fds = (int*) malloc(sizeof(int));
    bowman_fds = (int*) malloc(sizeof(int));
    poole_fds[0] = -1;
    bowman_fds[0] = -1;
    poole_connections = (Poole*) malloc(sizeof(Poole));

    poole_fds[0] = setupSocket(config.ip_poole, config.port_poole, poole_fds[0]);
    bowman_fds[0] = setupSocket(config.ip_bowman, config.port_bowman, poole_fds[0]);

    if (poole_fds[0] == -1 || bowman_fds[0] == -1) {
        return ERR;
    }
    FD_ZERO(&poole_set);
    FD_ZERO(&bowman_set);
    FD_SET(poole_fds[0],&poole_set);
    FD_SET(bowman_fds[0],&bowman_set);

    while (1) {
        num_poole_fds = process_requests(poole_fds, num_poole_fds, poole_set, 0);
        num_bowman_fds = process_requests(bowman_fds, num_bowman_fds, bowman_set, 1);
    }

    free(config.ip_poole);
    free(config.ip_bowman);
    free(buffer);
    //close(server_poole_fd); close(server_bowman_fd);

    return 0;

}