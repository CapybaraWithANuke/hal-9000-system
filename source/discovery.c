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

typedef struct {
    char* ip_poole;
    int port_poole;
    char* ip_bowman;
    int port_bowman;
} Config;

typedef struct {
    char* name;
    char* ip;
    char* port;
    int num_clients;
} Poole;

int* poole_fds;
int* bowman_fds;
int num_poole_fds = 1;
int num_bowman_fds = 1;
Poole* poole_connections;
int num_pooles = 0;

void leave(int signum){
    
    if (signum == SIGINT){

        free(bowman_fds);
        free(poole_fds);

        for (int i=0; i<num_pooles; i++){
            free(poole_connections[i].name);
            free(poole_connections[i].ip);
            free(poole_connections[i].port);
        }
        free(poole_connections);
    }
    exit(-1);
}

int setupSocket (char* ip, int port, int server_fd) {

    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        logn(ERR_BIND);
        return ERR;
    }

    if (listen(server_fd, 5) < 0) {
        logn(ERR_LISTEN);
        return ERR;
    }
    logn("Socket set up");

    return server_fd;
}

void poole_request(int fd,int pos) {

    Packet packet;

    do {
        packet = read_packet(fd);
        debug("PACKET READ");
        
        if (!strcmp(packet.header, "NEW_POOLE")){
            debug("BEFORE SPLIT");

            poole_connections = (Poole*) realloc (poole_connections, sizeof(Poole)*(++num_pooles));
            
            poole_connections[num_pooles-1].name = strtok(packet.data, "&");
            poole_connections[num_pooles-1].ip = strtok(NULL, "&");
            poole_connections[num_pooles-1].port = strtok(NULL, "&");
            poole_connections[num_pooles-1].num_clients = 0;
            debug("AFTER SPLIT");

            free(packet.header);
            free(packet.data);
            send_packet(fd, 1, "CON_OK", "");
        }
        else if (!strcmp(packet.header, "EXIT_POOLE")){

            for (int i=pos; i<num_poole_fds-1; i++){
                poole_fds[i] = poole_fds[i+1];
            }
            poole_fds = (int*) realloc(poole_fds, (--num_poole_fds)*sizeof(int));
            send_packet(fd, 1, "CON_OK", "");
            close(fd);
        }
        else {
            send_packet(fd, 1, "CON_KO", "");
        }
    } while (strcmp(packet.header, "EXIT_POOLE") && strcmp(packet.header, "EXIT_POOLE"));
}

void redirect_bowman(int fd, int pos) {

    int lowest = 2147483647;
    int lowest_index = 0;

    debug("REDIRECTING...");

    for (int i=1; i<num_poole_fds; i++){
        if (poole_connections[i].num_clients <= lowest) {
            lowest_index = i;
            lowest = poole_connections[i].num_clients;
        }
    }
    debug("Af6ter finding lowest");
    if (lowest_index > 0) {
        char* buffer;

        asprintf(&buffer, "%s&%s&%s", poole_connections[lowest_index].name, poole_connections[lowest_index].ip, poole_connections[lowest_index].port);
        send_packet(fd, 1, "CON_OK", buffer);
        free(buffer);

        for (int i=pos; i<num_bowman_fds; i++){
            
            if (pos != num_bowman_fds - 1){
                bowman_fds[pos] = bowman_fds[pos+1];
            }
        }
        num_bowman_fds--;
        bowman_fds = (int*) realloc(bowman_fds, sizeof(int)*num_bowman_fds);   
    }
    else {
        send_packet(fd, 1, "CON_KO", "");
    }
}

void process_requests (int* fds, int* num_fds, fd_set* set, int bowman_npoole) {

    int i, aux;
    int max_fd = 0;
    struct timeval timeout = {2, 0};
    int received_connection = 0;

    for (i=0; i<*num_fds; i++){
        if (fds[i] > max_fd){
            max_fd = fds[i];
        }
    }

    FD_ZERO(set);
    for (i=0; i<*num_fds; i++){
        FD_SET(fds[i], set);
    }

    if (select(max_fd + 1, set, 0, 0, &timeout) > 0) {
        for (i=0; i<*num_fds; i++) {

            if (FD_ISSET(fds[i], set)) {
                if (i == 0){
                    received_connection = 1;

                    if ((aux = accept(fds[0], NULL, 0)) > 0) {

                        fds = (int*) realloc(fds, sizeof(int)*(*num_fds + 1));
                        fds[*num_fds] = aux;
                        (*num_fds)++;

                        if (!bowman_npoole) {
                            logn("Received poole connection");
                        }
                        else {
                            logn("Received bowman connection");
                        }
                    }
                    else if (bowman_npoole) {
                        logn("ERROR accepting bowman client connection.");
                        send_packet(fds[i], 1, "CON_KO", "");
                    }
                    else {
                        logn("ERROR accepting poole server connection.");
                        send_packet(fds[i], 1, "CON_KO", "");
                    }
                }
                else {
                    if (!bowman_npoole){
                        logn("Received poole request");
                        poole_request(fds[i], i);                        
                    }
                    else {
                        logn("Received bowman request");
                        redirect_bowman(fds[i], i);
                    }
                }
            }
            if (received_connection){
                return;
            }
        }
    }
}

int main (int argc, char** argv) {

    Config config;
    int file_fd;
	const char* file_path_start = "config/";
    char* file_path = NULL;
    char* buffer = NULL;
    fd_set poole_set;
    fd_set bowman_set;

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
    buffer = read_until(file_fd, '\n');
    config.port_poole = atoi(buffer);
    free(buffer);
    config.ip_bowman = read_until(file_fd, '\n');
    buffer = read_until(file_fd, '\n');
    config.port_bowman = atoi(buffer);
    free(buffer);
    close(file_fd);
    free(file_path);

    poole_fds = (int*) malloc(sizeof(int));
    bowman_fds = (int*) malloc(sizeof(int));
    poole_fds[0] = -1;
    bowman_fds[0] = -1;
    poole_connections = (Poole*) malloc(sizeof(Poole));

    poole_fds[0] = setupSocket(config.ip_poole, config.port_poole, poole_fds[0]);
    bowman_fds[0] = setupSocket(config.ip_bowman, config.port_bowman, bowman_fds[0]);
    free(config.ip_poole);
    free(config.ip_bowman);

    FD_ZERO(&poole_set);
    FD_ZERO(&bowman_set);
    FD_SET(poole_fds[0],&poole_set);
    FD_SET(bowman_fds[0],&bowman_set);

    signal(SIGINT, leave);

    if (poole_fds[0] == -1 || bowman_fds[0] == -1) {
        return ERR;
    }

    while (1) {
        process_requests(poole_fds, &num_poole_fds, &poole_set, 0);
        process_requests(bowman_fds, &num_bowman_fds, &bowman_set, 1);
    }

    return 0;

}