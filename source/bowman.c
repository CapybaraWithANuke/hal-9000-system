#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ioctl.h>
#include <math.h>

#include "../header/commonfuncs.h"

#define print(x, y) write(x, y, strlen(y));
#define LOG_OUT -3
#define INVALID_COMMAND -2
#define UNKNOWN_COMMAND -1
#define CONNECT 0
#define LOGOUT 1
#define DOWNLOAD 2
#define LIST_SONGS 3
#define LIST_PLAYLISTS 4
#define CHECK_DOWNLOADS 5
#define CLEAR_DOWNLOADS 6

typedef struct{
	char* user; 
	char* directory;
	char* ip;
	char* port;
} Config;

int server_fd = -1;
Config config;

void leave(){

    free(config.user);
    free(config.directory);
    free(config.ip);
	free(config.port);
	if (server_fd > 0){
		close(server_fd);
	}
	exit (0);
}

int check_argument(int* i, int command,  char* string){

	if (command != DOWNLOAD){

		if (string[*i] == '\0'){
			return command;
		}
		else if (string[*i] != ' '){
			return INVALID_COMMAND;
		}
		else {
			return UNKNOWN_COMMAND;
		}
	}
	else {

		if (string[*i] == '\0'){
			return UNKNOWN_COMMAND;
		}
		else if (string[*i] != ' '){
			return INVALID_COMMAND;
		}
		else {
			int num_spaces = 0;

			for (int j = 0; string[j] != '\0'; j++){
				if (string[j] == ' ' && string[j+1] != '\0'){
					num_spaces++;
				}
			}
			if (num_spaces > 1){
				return UNKNOWN_COMMAND;
			}
			else {
				return command;
			}
		}
	}

}

int check_command(char* string){
	char commands[7][16] = {"CONNECT\0", "LOGOUT\0", "DOWNLOAD\0", "LIST SONGS\0", "LIST PLAYLISTS\0", "CHECK DOWNLOADS\0", "CLEAR DOWNLOADS\0"};
	int could_be_same[7] = {1, 1, 1, 1, 1, 1, 1};
	int command_num_spaces[7] = {0, 0, 0, 1, 1, 1, 1};
	int num_spaces[7] = {0, 0, 0, 0, 0, 0, 0};
	int action;
	int i = 0, j = 0;

	if (strlen(string) < 6){
		return INVALID_COMMAND;
	}

	while (string[j] != '\0'){
        
        if (!(string[j] == ' ' && string[j+1] == ' ')){
            string[i] = string[j];
            i++;
        }
        j++;
    }
	if (string[i-1] == ' '){
		string[i-1] = '\0';
	}
	else {
		string[i] = '\0';
	}
	i = 0;
	j = 0;

	while (string[i] != '\0'){
		for (j = 0; j < 7; j++){

			if (could_be_same[j] == 1){
				if (!(commands[j][i] == string[i] || (commands[j][i] == string[i] + ('A' - 'a') && string[i] >= 'a' && string[i] <= 'z'))){
					could_be_same[j] = 0;
				}
				else {
					
					if (string[i+1] == '\0' || string[i+1] == ' '){

						if (could_be_same[j] == 1 && command_num_spaces[j] == num_spaces[j]){
							i++;
							
							if (j != DOWNLOAD && !(commands[j][i] == string[i] || (commands[j][i] == string[i] + ('A' - 'a') && string[i] >= 'a' && string[i] <= 'z'))){
								return INVALID_COMMAND;
							}
							action = check_argument(&i, j, string);
							if (action >= 0){
								return j;
							}
							return action;
						}

						if (string[i+1] == ' ')	{
							num_spaces[j]++;
						}
					}
				}
			}
		}
		i++;
	}
	return INVALID_COMMAND;
}

int system_connect() {

	struct sockaddr_in server;
	char* buffer;

	bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(config.port));
    if(inet_pton(AF_INET, config.ip, &server.sin_addr) < 0) {
        print(2, "Error connecting to the server\n");
		debug("GVHBJNKMNB ");
        return -1;
    }
    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        print(2, "Error connecting to the server\n");
        return -1;
    }
    if(connect(server_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
        print(2, "Error connecting to the server\n");
        close(server_fd);
        return -1;
    }
	send_packet(server_fd, 1, "NEW_BOWMAN", config.user);

	Packet packet = read_packet(server_fd);

	if (!strcmp(packet.header,"CON_OK")) {
		
		close(server_fd);
		free(packet.header);
		free(packet.data);
		
		char* name_string = strtok(packet.data, "&");
		char* ip_string = strtok(packet.data, "&");
		char* port_string = strtok(packet.data, "&");

		bzero(&server, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(atoi(port_string));
		
		if(inet_pton(AF_INET, ip_string, &server.sin_addr) < 0) {
			print(2, "Error connecting to the server\n");
			free(name_string);
			free(ip_string);
			free(port_string);
			return -1;
		}
		free(name_string);
		free(ip_string);
		free(port_string);

		if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			print(2, "Error connecting to the server\n");
			return -1;
		}
		if(connect(server_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
			print(2, "Error connecting to the server\n");
			close(server_fd);
			return -1;
		}
		send_packet(server_fd, 1, "NEW_BOWMAN", config.user);

		Packet packet = read_packet(server_fd);

		if (!strcmp(packet.header,"CON_OK")) {
			free(packet.header);
			free(packet.data);
			
			asprintf(&buffer, "%s connected to HAL 9000 system, welcome music lover!", config.user);
			logn(buffer);
			free(buffer);
			return server_fd;
		}
	}
	free(packet.header);
	free(packet.data);
	print(2, "Error connecting to the server\n");
	return -1;
}

int main(int argc, char *argv[]){
	int command = 0;
	char* string = NULL;
	const char* file_path_start = "config/";
    char* file_path = NULL;
	int server_fd = -1;
	
	signal(SIGINT, leave);

    if (argc != 2){
        print(2, "Error: There must be exactly one parameter when running\n");
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

	config.user = read_until(fd_config, '\n');
    remove_symbol(config.user, '&');

	config.directory = read_until(fd_config, '\n');
	config.ip = read_until(fd_config, '\n');
	config.port = read_until(fd_config, '\n');
	remove_symbol(config.directory, '\r');
	remove_symbol(config.ip, '\r');
	remove_symbol(config.port, '\r');
	close(fd_config);

	asprintf(&string, "%s user has been initialized\n\n", config.user);
	print(1, string);
	free(string);

	while (command != LOG_OUT){
		print(1, "$ ");
		string = read_until(0, '\n');
		command = check_command(string);
		free(string);
		if (command == INVALID_COMMAND){
			print(2, "ERROR: Please input a valid command.\n");
		}
		else if (command == UNKNOWN_COMMAND){
			print(2, "Unknown command.\n");
		}
		else {
			if (command == CONNECT && server_fd > 0){
				logn("Already connected");
				command = 10;
			}
			switch (command){
				case CONNECT :
					server_fd = system_connect();
					break;
				case LOGOUT :
					break;
				case DOWNLOAD :
					break;
				case LIST_SONGS :
					break;
				case LIST_PLAYLISTS :
					break;
				case CHECK_DOWNLOADS :
					break;
				case CLEAR_DOWNLOADS :
					break;
			}
		}
	}
	leave();
	
    return 0;
}