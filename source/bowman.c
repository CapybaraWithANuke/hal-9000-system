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
#include <pthread.h>

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

typedef struct {
	short id;
	char* filename;
	char* playlist;
	long filesize;
	long current_filesize;
	char* md5sum;
	int fd;
} Song;

int discovery_fd = -1;
int poole_fd = -1;
Config config;
Song* songs;
int num_songs = 0;
int input_length = 0;
char* playlist_name;
pthread_t thread;
char* input;

void exit_servers() {

	Packet packet;
	int i = 0;

	do {
		if (i!= 0) {
			free(packet.header);
			free(packet.data);
		}
        send_packet(discovery_fd, 6, "EXIT", "");
        packet = read_packet(poole_fd);
		i++;
    } while (strcmp(packet.header, "CONOK"));

	close(poole_fd);
	poole_fd = -1;
	free(packet.header);
	free(packet.data);
	if (poole_fd != -1){
		pthread_join (thread, NULL);
	}
	i = 0;

	do {
		if (i!= 0) {
			free(packet.header);
			free(packet.data);
		}
        send_packet(discovery_fd, 6, "EXIT_BOWMAN", "");
        packet = read_packet(discovery_fd);
		i++;
    } while (strcmp(packet.header, "CON_OK"));

    close(discovery_fd);
	discovery_fd = -1;
    free(packet.data);
    free(packet.header);
}

void leave(){

    free(config.user);
    free(config.directory);
    free(config.ip);
	free(config.port);
	if (discovery_fd > 0){
		exit_servers();
	}
	exit (-1);
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
        return -1;
    }
    if ((discovery_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        print(2, "Error connecting to the server\n");
        return -1;
    }
    if(connect(discovery_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
        print(2, "Error connecting to the server\n");
        close(discovery_fd);
        return -1;
    }
	send_packet(discovery_fd, 1, "NEW_BOWMAN", config.user);

	Packet packet = read_packet(discovery_fd);

	if (!strcmp(packet.header,"CON_OK")) {

		char* name_string = strtok(packet.data, "&");
		char* ip_string = strtok(NULL, "&");
		char* port_string = strtok(NULL, "&");

		bzero(&server, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(atoi(port_string));
		
		if(inet_pton(AF_INET, ip_string, &server.sin_addr) < 0) {
			print(2, "Error connecting to the server\n");
			free(name_string);
			free(ip_string);
			free(port_string);
			exit_servers();
			return -1;
		}
		free(packet.header);
		free(packet.data);

		if ((poole_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			print(2, "Error connecting to the server\n");
			exit_servers();
			return -1;
		}
		if(connect(poole_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
			print(2, "Error connecting to the server\n");
			exit_servers();
			close(poole_fd);
			return -1;
		}
		send_packet(poole_fd, 1, "NEW_BOWMAN", config.user);

		Packet packet = read_packet(poole_fd);

		if (!strcmp(packet.header,"CON_OK")) {
			free(packet.header);
			free(packet.data);
			
			asprintf(&buffer, "%s connected to HAL 9000 system, welcome music lover!", config.user);
			logn(buffer);
			free(buffer);
			return poole_fd;
		}
	}
	free(packet.header);
	free(packet.data);
	print(2, "Error connecting to the server\n");
	return -1;
}

void list_songs() {

	int i = 1;
	char** song_name = (char**) calloc(1, sizeof(char*));
	int num_songs = 0;
	int num_char = 0;

	input_length = 0;
	input = (char*) realloc(input, sizeof(char));
	input[0] = '\0';
	send_packet(poole_fd, 2, "LIST_SONGS", "");
	
	while (input_length == 0 || input[input_length-1] != '\0') {
	}

	song_name[0] = (char*) calloc(1, sizeof(char));

	for (i=0; 1; i++) {

		if (input[i] == '\0'){
			song_name[num_songs][num_char] ='\0';
			break;
		}
		else if (input[i] == '&'){
			song_name[num_songs][num_char] ='\0';
			num_char = 0;
			song_name = (char**) realloc(song_name, (++num_songs + 1)*sizeof(char*));
			song_name[num_songs] = (char*) calloc(1, sizeof(char));
		}
		else {
			song_name[num_songs][num_char] = input[i];
			song_name[num_songs] = (char*) realloc(song_name[num_songs], (++num_char + 1)*sizeof(char));
		}
	}
	log("There are "); 
	logi(num_songs); 
	logn(" songs available for download:");

	for (i=0; i<num_songs; i++) {
		logi(i+1); log(". "); log(song_name[i]); logn("");
		free(song_name[i]);
	}

	free(song_name);
	free(input);
	input_length = 0;
}

void list_playlists() {

	int i = 1;
	char*** playlists = (char***) calloc(1, sizeof(char**));
	int* num_songs = (int*) calloc(1, sizeof(int));
	int num_playlists = 0;
	int num_char = 0;
	char* buffer;

	input_length = 0;
	input = (char*) realloc(input, sizeof(char));
	input[0] = '\0';
	send_packet(poole_fd, 2, "LIST_PLAYLISTS", "");
	
	while (input_length == 0 || input[input_length-1] != '\0') {
		log("input: "); debug(input);
		sleep(2);
	}

	playlists[0] = (char**) calloc(1, sizeof(char*));
	playlists[0][0] = (char*) calloc(1, sizeof(char));

	for (i=0; 1; i++) {
		write(1, &input[i], sizeof(char));

		if (input[i] == '\0'){
			playlists[num_playlists][num_songs[num_playlists]][num_char] ='\0';
			num_songs[num_playlists]++;
			num_playlists++;
			break;
		}
		else if (input[i] == '#') {
			playlists[num_playlists][num_songs[num_playlists]][num_char] ='\0';
			num_char = 0;
			num_songs[num_playlists]++;
			playlists = (char***) realloc(playlists, (++num_playlists + 1)*sizeof(char**));
			playlists[num_playlists] = (char**) calloc(1, sizeof(char*));
			playlists[num_playlists][0] = (char*) calloc(1, sizeof(char));
			num_songs[num_playlists] = 0;
		}
		else if (input[i] == '&'){
			playlists[num_playlists][num_songs[num_playlists]][num_char] ='\0';
			num_char = 0;
			playlists[num_playlists] = (char**) realloc(playlists[num_playlists], (++num_songs[num_playlists] + 1)*sizeof(char));
			playlists[num_playlists][num_songs[num_playlists]] = (char*) calloc(1, sizeof(char));

		}
		else {
			playlists[num_playlists][num_songs[num_playlists]][num_char] = input[i];
			playlists[num_playlists][num_songs[num_playlists]] = (char*) realloc(playlists[num_playlists][num_songs[num_playlists]], (++num_char + 1)*sizeof(char));
		}
	}
	log("There are "); logi(num_playlists); logn(" lists available for download:");

	for (i=0; i<num_playlists; i++) {
		logi(i+1); log(". "); logn(playlists[i][0]);

		for (int j=1; j<num_songs[i]; j++) {
			asprintf(&buffer, "\t%c. %s\n", (char)('a'+j-1), playlists[i][j]);
			log(buffer);
			free(buffer);
		}
	}	
	for (i=0; i<num_playlists; i++) {
		log("i: ");logni(i);
		for (int j=0; j<num_songs[i]; j++) {
			log("i: ");logi(i); log("   j: ");logni(j);
			free(playlists[i][j]);
		}
		free(playlists[i]);
	}	
	free(playlists);
	free(num_songs);
	free(input);
	input_length = 0;
}

void download(char* string) {

	char* argument = (char*) calloc(1, sizeof(char));
	int i, j;

	for (i=0; string[i-1] != ' ' ; i++);

	for(j=0; string[i] != '\0'; i++) {
		argument[j] = string[i];
		argument = (char*) realloc(argument, (++j + 1)*sizeof(char));
	}
	argument[j] = '\0';

	if (strlen(argument) > 4 && argument[strlen(argument)-4] == '.' && argument[strlen(argument)-3] == 'm' && 
		argument[strlen(argument)-2] == 'p' && argument[strlen(argument)-1] == '3') {

		playlist_name = NULL;
		send_packet(poole_fd, 3, "DOWNLOAD_SONG", argument);
	}
	else {
		playlist_name = argument;
		send_packet(poole_fd, 3, "DOWNLOAD_LIST", argument);
	}
}

void logout() {

	Packet packet;
	int i = 0;

	do {
		if (i != 0){
			free(packet.header);
			free(packet.data);
		}
        send_packet(poole_fd, 6, "EXIT", "");
        packet = read_packet(poole_fd);
		i++;
    } while (strcmp(packet.header, "CONOK"));

	close(poole_fd);
	poole_fd = -1;

	do {
		free(packet.header);
		free(packet.data);

        send_packet(poole_fd, 6, "EXIT_BOWMAN", "");
        packet = read_packet(poole_fd);
    } while (strcmp(packet.header, "CON_OK"));

	close(discovery_fd);
	discovery_fd = -1;
	free(packet.header);
	free(packet.data);
}

void receive_file(Packet packet) {

	char* path;

	if (!strcmp(packet.header, "NEW_FILE")){
		path = "../bowman_music/song_files/";

		songs = (Song*) realloc(songs, ++num_songs*sizeof(Song));

		songs[num_songs-1].filename = strtok(packet.data, "&");
		songs[num_songs-1].filesize = strtol(strtok(NULL, "&"), NULL, 10);
		songs[num_songs-1].current_filesize = 0;
		songs[num_songs-1].md5sum = strtok(NULL, "&");
		songs[num_songs-1].id = atoi(strtok(NULL, "&"));
		if (playlist_name == NULL){
			songs[num_songs-1].playlist = NULL;
		}
		else {
			songs[num_songs-1].playlist = (char*) calloc(strlen(playlist_name)+1, sizeof(char));
			strcpy(songs[num_songs-1].playlist, playlist_name);
		}
		path = (char*) realloc(path, (strlen(path)+1+strlen(songs[num_songs-1].filename))*sizeof(char));
		strcat(path, songs[num_songs-1].filename);

		if (((songs[num_songs-1].fd) = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
			free(songs[num_songs-1].filename);
			if (playlist_name != NULL){
				free(songs[num_songs-1].playlist);
			}
			songs = (Song*) realloc(songs, --num_songs*sizeof(Song));
		}
		free(path);
	}
	else if (!strcmp(packet.header, "FILE_DATA")) {
		char* temp_id = strtok(packet.data, "&");
		short id = temp_id[0]*256 + temp_id[1];
		char* data = strtok(NULL, "&");
		int ffd, i;
		int data_size = 256 - 3 - strlen("FILE_DATA");

		for (i=0; i<1000; i++) {
			if (songs[i].id == id) {
				break;
			}
		}
		ffd = songs[i].fd;
		write(ffd, data, strlen(data));
		free(data);
		songs[i].current_filesize = songs[i].current_filesize + strlen(data);

		path = "../bowman_music/song_files/";
		path = (char*) realloc(path, (strlen(path)+1+strlen(songs[i].filename))*sizeof(char));
		strcat(path, songs[i].filename);

		if (data[data_size-1] == '\0') {

			close(ffd);
			char* md5sum = get_md5sum(path);

			if (!strcmp(songs[i].md5sum, md5sum)) {
				send_packet(poole_fd, 5, "CHECK_OK", "");
			}
			else {
				send_packet(poole_fd, 5, "CHECK_KO", "");
			}
			free(md5sum);
		}
	}
	else if (!strcmp(packet.header, "NOT_EXISTS")) {
		
	}
}

void* process_requests() {
	
	Packet packet;
	int data_size;
	fd_set set;
	int i;

	while (1){
		FD_ZERO(&set);
		FD_SET(poole_fd, &set);

		select(poole_fd + 1, &set, NULL, NULL, NULL);

		if (FD_ISSET(poole_fd, &set)) {

			packet = read_packet(poole_fd);
			data_size = 256 - 3 - packet.header_length;

			switch (packet.type){
			case 2:
				if (!strcmp(packet.header, "SONGS_RESPONSE")) {

					input = (char*) realloc(input, (input_length+data_size+1)*sizeof(char));
					for (i=input_length; i<input_length+data_size+1; i++) {
						input[i] = '\0';
					}
					strcat(input, packet.data);
					input_length = input_length + data_size;
				}
				break;
			case 4:
				receive_file(packet);
				break;
			default:
				break;
			}
			free(packet.header);
			free(packet.data);
		}
		
	}
	return NULL;
}

void check_downloads() {

	int percent;
	int space;
	char buffer[31];

	for (int i=0; i<num_songs; i++) {
		percent = songs[i].current_filesize * 100 / songs[i].filesize;
		sprintf(buffer, "%d", percent);
		space = 30 - 4 - strlen(buffer);

		buffer[strlen(buffer)] = '%';
		buffer[strlen(buffer)+1] = ' ';
		buffer[strlen(buffer)+2] = '|';
		int char_start = strlen(buffer) - 3;

		for (int i=char_start; i<29; i++) {

			if (i - char_start < (percent * space / 100) ) {
				buffer[i] = '=';
			}
			else if (i - char_start == (percent * space / 100) ) {
				buffer[i] = '%';
			}
			else {
				buffer[i] = ' ';
			}
		}
		buffer[i] = '|';
		buffer[i+1] = '\0';

		if (songs[i].playlist != NULL) {
			log(songs[i].playlist);
			log(" - ");
		}
		logn(songs[i].filename);
		log("\t\t");
		logi(percent);
		logn(buffer);
	}
}

void clear_downloads() {

	int i, j = 0;

	for (i=0; i<num_songs; i++) {
		if (songs[i].current_filesize >= songs[i].filesize) {
			j++;
		}
		if (j > 0) {
			songs[i].id = songs[j].id;
			songs[i].filename = songs[j].filename;
			songs[i].playlist = songs[j].playlist;
			songs[i].filesize = songs[j].filesize;
			songs[i].current_filesize = songs[j].current_filesize;
			songs[i].md5sum = songs[j].md5sum;
			songs[i].fd = songs[j].fd;
		}
		else if (i + j >= num_songs) {
			free(songs[i].filename);
			free(songs[i].playlist);
		}
	}
	num_songs = num_songs - j;
	songs = (Song*) realloc(songs, sizeof(Song)*num_songs);
}

int main(int argc, char *argv[]){
	int command = 0;
	char* string = NULL;
	const char* file_path_start = "config/";
    char* file_path = NULL;
	
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
	remove_symbol(config.user, '\r');

	config.directory = read_until(fd_config, '\n');
	config.ip = read_until(fd_config, '\n');
	config.port = read_until(fd_config, '\n');
	remove_symbol(config.directory, '\r');
	remove_symbol(config.ip, '\r');
	remove_symbol(config.port, '\r');
	close(fd_config);

	asprintf(&string, "%s user has been initialized", config.user);
	logn(string);
	free(string);

	while (command != LOG_OUT){
		print(1, "\n$ ");
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
			if (command == CONNECT && poole_fd != -1){
				logn("Already connected");
				command = 10;
			}
			if (command != CONNECT && poole_fd == -1){
				logn("Need to be connected for this command");
				command = 10;
			}
			switch (command){
				case CONNECT :
					poole_fd = system_connect();
					pthread_create (&thread, NULL, process_requests, NULL);
					break;
				case LIST_SONGS :
					list_songs();
					break;
				case LIST_PLAYLISTS :
					list_playlists();
					break;
				case DOWNLOAD :
					download(string);
					break;
				case CHECK_DOWNLOADS :
					check_downloads();
					break;
				case CLEAR_DOWNLOADS :
					clear_downloads();
					break;
				case LOGOUT :
					logout();
					break;
			}
		}
	}
	leave();
	
    return 0;
}