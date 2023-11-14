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
	int port;
} Bowman;

Bowman bowman;

void free_everything(){
    free(bowman.user);
    free(bowman.directory);
    free(bowman.ip);
}

void signal_manage(__attribute__((unused)) int signum){
	free_everything();
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

int process_argument(int command){
	switch (command){
		case CONNECT :
			break;
		case LOGOUT :
			return LOG_OUT;
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
	return command;
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
								return process_argument(j);
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

void remove_ampersand(char* string){
	int i1 = 0, i2 = 0;
	while (string[i2] != '\0'){
        
        if (string[i2] != '&'){
            string[i1] = string[i2];
            i1++;
        }
        i2++;
    }
    string[i1] = '\0';
}

int main(int argc, char *argv[]){
	int action = 0;
	char* string = NULL;
	const char* file_path_start = "config/";
    char* file_path = NULL;
	
	signal(SIGINT, signal_manage);

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

	bowman.user = read_until(fd_config, '\n');
    remove_ampersand(bowman.user);

	bowman.directory = read_until(fd_config, '\n');
	bowman.ip = read_until(fd_config, '\n');
	string = read_until(fd_config, '\n');
	bowman.port = atoi(string);
	free(string);
	close(fd_config);

	asprintf(&string, "%s user has been initialized\n\n", bowman.user);
	print(1, string);
	free(string);

	while (action != LOG_OUT){
		print(1, "$ ");
		string = read_until(0, '\n');
		action = check_command(string);
		free(string);
		if (action == INVALID_COMMAND){
			print(2, "ERROR: Please input a valid command.\n");
		}
		else if (action == UNKNOWN_COMMAND){
			print(2, "Unknown command.\n");
		}
	}
	free_everything();
	
    return 0;
}