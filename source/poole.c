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
#include <math.h>

#include "../header/commonfuncs.h"

#define print(x) write(1, x, strlen(x));

#define ERR_SOCKET "Error: socket not created"
#define ERR_CONNECT "Error: connection not established"
#define ERR_BIND "Error: binding failed"
#define ERR_LISTEN "Error: listening failed"
#define ERR_ACCEPT "Error: accepting failed"

#define ERR -1

#define PLAYLIST_COUNT 3
#define SONG_COUNT 5
#define MAX_SONG_LENGTH 256


typedef struct{
	char* server_name; 
	char* directory;
	char* discovery_ip;
	int discovery_port;
    char* poole_ip;
	int poole_port;
} Poole;

typedef struct {
    char* name;
    int num_songs;
    char** songs;
} Playlist;

Poole poole;
const char songs[SONG_COUNT][MAX_SONG_LENGTH] = {"despacito", "sway", "gimme chocolate", "oregon boyz", "nowhere to go"};
Playlist playlists[PLAYLIST_COUNT];

void initializePlaylists() {

    playlists[0].name = "Dolores";
    playlists[1].name = "Mildred";
    playlists[2].name = "Flux";

    playlists[0].num_songs = 2;
    playlists[1].num_songs = 3;
    playlists[2].num_songs = 2;

    playlists[0].songs = (char**) malloc(sizeof(char*)*2);
    playlists[1].songs = (char**) malloc(sizeof(char*)*3);
    playlists[2].songs = (char**) malloc(sizeof(char*)*2);

    playlists[0].songs[0] = "despacito";
    playlists[0].songs[1] = "sway";

    playlists[1].songs[0] = "idk";
    playlists[1].songs[1] = "what to invent";
    playlists[1].songs[2] = "anymore";
    
    playlists[2].songs[0] = "it's 6 AM";
    playlists[2].songs[1] = "a.";

}

void free_everything(){
    free(poole.server_name);
    free(poole.directory);
    free(poole.discovery_ip);
    free(poole.poole_ip);
}

int setupSocket(char* ip, int port, int server_fd) {

    struct sockaddr_in address;
    socklen_t addrlen;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);
    addrlen = sizeof(address);

    if (bind(server_fd, (struct sockaddr*)&address, addrlen) < 0) {
        logn(ERR_BIND);
        return ERR;
    }

    if (listen(server_fd, 5) < 0) {
        logn(ERR_LISTEN);
        return ERR;
    }

    return 1;

}

void listSongs(int* sockets_fd, int i) {

    char* data = (char*) calloc((SONG_COUNT * 2 - 1) + 1, sizeof(char));

    for (int j = 0; j < SONG_COUNT; ++j) {
        strcat(data, songs[j]);
        if (j != (SONG_COUNT-1)) strcat(data, "&");
    }
    
    //One additional byte reserved for the number of frames
    int num_frames = ceil(strlen(data)*1.0/(256 - 4 - strlen("SONGS_RESPONSE")));
    int data_index = 0;

    for (int j = 0; j < num_frames; ++j) {

        char* buffer = (char*) calloc((256 - 3 - strlen("SONGS_RESPONSE"))+1, sizeof(char));
        
        if (j == 0) {
            buffer[0] = num_frames + '0';
            while ((strlen(buffer) <= (256 - 3 - strlen("SONGS_RESPONSE"))) && data[data_index] != '\0') {
                char aux[2];
                aux[0] = data[data_index];
                aux[1] = '\0';
                strcat(buffer, aux);
                ++data_index;
            }
        
            send_packet(2, strlen("SONGS_RESPONSE"), "SONGS_RESPONSE", buffer);
            
        }
        else {
            while ((strlen(buffer) <= (256 - 3 - strlen("SONGS_RESPONSE"))) && data[data_index] != '\0') {
                char aux[2];
                aux[0] = data[data_index];
                aux[1] = '\0';
                strcat(buffer, aux);
                ++data_index;
            }

            send_packet(sockets_fd[i], 2, "SONGS_RESPONSE", buffer);

        }

    }


}

void listPlaylists(int* sockets_fd, int i) {

    char* data = (char*) calloc(2, sizeof(char));

    for (int j = 0; j < PLAYLIST_COUNT; ++j) {

        strcat(data, playlists[j].name);
        strcat(data, "&");

        for (int k = 0; k < playlists[j].num_songs; ++k) {

            strcat(data, playlists[j].songs[k]);


            if (k < (playlists[j].num_songs-1)) {
                strcat(data, "&");
            }
            else {
                strcat(data, "#");
            }

        }


    }

    int num_frames = ceil(strlen(data)/(256 - 4 - strlen("SONGS_RESPONSE")));
    char* buffer = (char*) calloc((256 - 3 - strlen("SONGS_RESPONSE") + 1), sizeof(char));
    int data_index = 0;
    for (int j = 0; j < num_frames; ++j) {

        if (j == 0) {
            buffer[0] = num_frames + '0';

            while (strlen(buffer) < (256 - 4 - strlen("SONGS_RESPONSE")) && data[data_index] != '\0') {
                char aux[2];
                aux[0] = data[data_index];
                aux[1] = '\0';
                strcat(buffer, aux);
                ++data_index;
            }
        }
        else {
            while (strlen(buffer) < (256 - 4 - strlen("SONGS_RESPONSE")) && data[data_index] != '\0') {
                char aux[2];
                aux[0] = data[data_index];
                aux[1] = '\0';
                strcat(buffer, aux);
                ++data_index;
            }
        }

        send_packet(sockets_fd[i], 2, "SONGS_RESPONSE", buffer);

        free(buffer);
        buffer = (char*) calloc((256 - 3 - strlen("SONGS_RESPONSE") + 1), sizeof(char));

    }

}

int main(int argc, char *argv[]) {

    initializePlaylists();
	
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
    remove_symbol(poole.server_name, '\r');
	poole.directory = read_until(fd_config, '\n');
    remove_symbol(poole.directory, '\r');
	poole.discovery_ip = read_until(fd_config, '\n');
    remove_symbol(poole.discovery_ip, '\r');
	string = read_until(fd_config, '\n');
    remove_symbol(string, '\r');
	poole.discovery_port = atoi(string);
    free(string);
    poole.poole_ip = read_until(fd_config, '\n');
    remove_symbol(poole.poole_ip, '\r');
    string = read_until(fd_config, '\n');
    remove_symbol(string, '\r');
	poole.poole_port = atoi(string);
    free(string);
    close(fd_config);

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(poole.discovery_port);
    if(inet_pton(AF_INET, poole.discovery_ip, &serv_addr.sin_addr) < 0) {
        logn("Error connecting to the server\n");
        return ERR;
    }
    if ((discovery_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        logn("Error connecting to the server\n");
        return ERR;
    }
    if (connect(discovery_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logn(ERR_CONNECT);
        return ERR;
    }

    char* frame_data = calloc(2, sizeof(char));
    strcat(frame_data, poole.server_name);
    strcat(frame_data, "&");
    strcat(frame_data, poole.poole_ip);
    strcat(frame_data, "&");
    char* buffer;
    asprintf(&buffer, "%d", poole.poole_port);
    strcat(frame_data, buffer);

    send_packet(discovery_fd, 1, "NEW_POOLE", frame_data);

    free(buffer);
    free(frame_data);

    Packet packet = read_packet(discovery_fd);

    while (packet.header[4] == 'K') {
        char* frame_data = calloc(2, sizeof(char));
        strcat(frame_data, poole.server_name);
        strcat(frame_data, "&");
        strcat(frame_data, poole.poole_ip);
        strcat(frame_data, "&");
        char* buffer;
        asprintf(&buffer, "%d", poole.poole_port);
        strcat(frame_data, buffer);
        free(buffer);
        
        send_packet(discovery_fd, 1, "NEW_POOLE", frame_data);

        free(frame_data);

        packet = read_packet(discovery_fd);
    }
    free(packet.data);
    free(packet.header);

    do {
        send_packet(discovery_fd, 6, "EXIT_POOLE", "");
        packet = read_packet(discovery_fd);
    } while (strcmp(packet.header, "CON_OK"));

    close(discovery_fd);
    free(packet.data);
    free(packet.header);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    struct sockaddr_in address;
    socklen_t addrlen;

    address.sin_family = AF_INET;
    address.sin_port = htons(poole.poole_port);
    address.sin_addr.s_addr = inet_addr(poole.poole_ip);
    addrlen = sizeof(address);

    fd_set set;
    FD_ZERO(&set);
    FD_SET(server_fd, &set);
    int* sockets_fd = (int*) calloc(1, sizeof(int));
    char** names_bowmans = (char**) calloc(1, sizeof(char*));
    int sockets_index = 0;
    int nfds = server_fd+1;

    setupSocket(poole.poole_ip, poole.poole_port, server_fd);

    while(69) {

        FD_ZERO(&set);
        FD_SET(server_fd, &set);
        for (int i = 0; i<sockets_index; i++) {
            FD_SET(sockets_fd[i], &set);
        }

        select(nfds, &set, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &set)) {

            if ((sockets_fd[sockets_index] = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
                logn(ERR_ACCEPT);
                return ERR;
            }
            FD_SET(sockets_fd[sockets_index], &set);
            ++sockets_index;
            sockets_fd = (int*) realloc(sockets_fd, sizeof(int)*(sockets_index + 1));
            names_bowmans = (char**) realloc(names_bowmans, sizeof(char*)*(sockets_index + 1));
        }

        for (int i = 0; i < sockets_index; ++i) {

            if (FD_ISSET(sockets_fd[i], &set)) {
                
                Packet packet = read_packet(sockets_fd[i]);

                switch((int) packet.type) {

                    case 1:
                        names_bowmans[i] = (char*) calloc(strlen(packet.data)+1, sizeof(char));
                        strcpy(names_bowmans[i], packet.data);

                        send_packet(sockets_fd[i], 1, "CON_OK", "");
                        break;
                    case 2:
                        if (!strcmp(packet.header, "LIST_SONGS")) {
                            listSongs(sockets_fd, i);
                        }

                        else if (!strcmp(packet.header, "LIST_PLAYLISTS")) {
                            listPlaylists(sockets_fd, i);
                        }

                        break;

                }
            }
        }
    }

    free_everything();
	
    return 0;
}