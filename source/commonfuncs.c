#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "../header/commonfuncs.h"

char* read_until(int fd, char end) {

	char* string = (char*) malloc(sizeof(char));
	char c = 0;
	int i = 0, size = 0;

	while (1) {
		size = read(fd, &c, sizeof(char));
		if (c != end && size > 0) {
			string = (char *) realloc(string, sizeof(char)*(i + 2));
			string[i++] = c;
		}
		else{
			break;
		}
	}
	string[i] = '\0';
	return string;
}

char * read_until2(int fd, char end1, char end2) {
	char *string = string = (char *) malloc(sizeof(char));
	char c;
	int i = 0, size;

	while (1) {
		size = read(fd, &c, sizeof(char));
		if(c != end1 && c != end2 && size > 0) {
			string = (char *) realloc(string, sizeof(char)*(i + 2));
			string[i++] = c;
		}
		else {
			break;
		}
	}
	string[i] = '\0';
	debug("");
	return string;
}

void logn(char* x) {
	char* x_new = (char*) calloc(strlen(x)+2, sizeof(char));
    strcat(x_new, x);
    strcat(x_new, "\n");
    log(x_new);
    free(x_new);
}

void debug(char* x) {
	char* x_new = (char*) calloc(strlen(x)+2, sizeof(char));
    strcat(x_new, x);
    strcat(x_new, "\n");
    log(x_new);
    free(x_new);
}

void logi(int x) {
    char* buffer;
    asprintf(&buffer, "%d", x);
    log(buffer);
    free(buffer);
}

void logni(int x) {
    char* buffer;
    asprintf(&buffer, "%d\n", x);
    log(buffer);
    free(buffer);
}

/*Packet read_packet(int fd) {

	Packet new_packet;
	short data_length;

	read(fd, &(new_packet.type), sizeof(char));

	read(fd, &(new_packet.empty), sizeof(char));
	read(fd, &(new_packet.empty), sizeof(char));
	new_packet.header_length = (int) new_packet.empty;

	new_packet.header = (char*) calloc((new_packet.header_length+1), sizeof(char));
	data_length = 256 - 3 - new_packet.header_length;
	new_packet.data = (char*) calloc((data_length+1), sizeof(char));

	read(fd, new_packet.header, (new_packet.header_length) * sizeof(char));
	new_packet.header[new_packet.header_length] = '\0';

	read(fd, new_packet.data, data_length * sizeof(char));
	new_packet.data[data_length] = '\0';

	return new_packet;
}*/

Packet read_packet(int fd) {
	Packet new_packet;
	short data_length;
	short i;

	read(fd, &new_packet.type, sizeof(char));

	read(fd, &new_packet.empty, sizeof(char));
	read(fd, &new_packet.empty, sizeof(char));
	new_packet.header_length = (int) new_packet.empty;

	new_packet.header = (char*) calloc(new_packet.header_length+1, sizeof(char));
	data_length = 256 - 3 - new_packet.header_length;
	new_packet.data = (char*) calloc(data_length+1, sizeof(char));

	for (i=0; i<new_packet.header_length; i++){
		read(fd, &new_packet.header[i], sizeof(char));
	}
	new_packet.header[i] = '\0';

	for (i=0; i<data_length; i++){
		read(fd, &new_packet.data[i], sizeof(char));
	}
	new_packet.data[i] = '\0';
	
	return new_packet;
}

void fill_with(char symbol, char* data, int size) {

	for (int i=0; i<size; i++){
		data[i] = symbol;
	}
}

/*void send_packet(int fd, int type, char* header, char*data) {

	int i = 0;
	int data_length = 256 - 3 - strlen(header);
	int bytes_left = strlen(data)+1;
	char* new_data = data;
	char* packet = (char*) calloc(256, sizeof(char));

	do {
		packet[0] = (char) type;
		packet[1] = (char) 0;
		packet[2] = (char) strlen(header);
		packet[3] = '\0';
		strcat(packet, header);

		if (bytes_left <= data_length+1) {
			
			for (i=0; i<(int)strlen(new_data); i++) {
				packet[3+strlen(header)+i] = new_data[i];
			}
			for (i=strlen(new_data); i<data_length; i++) {
				packet[i] = '\0';
			}
		}
		else {
			for (i=0; i<data_length; i++) {
				packet[3+strlen(header)+i] = new_data[i];
			}
		}
		write(fd, packet, 256);

		bytes_left = bytes_left - data_length;
		if (bytes_left > 0)	{
			new_data = new_data + data_length;
		}
		log("packet sent: "); debug(packet);
	} while(bytes_left > 0);

	free(packet);
}*/

void send_packet(int fd, int type, char* header, char*data) {

	char aux;
	int data_length = 256 - 3 - strlen(header);
	int bytes_left = strlen(data)+1;
	char* new_data = data;

	do {
		aux = (char) type;
		write(fd, &aux, sizeof(char));
		aux = (char) 0;
		write(fd, &aux, sizeof(char));
		aux = (char) strlen(header);
		write(fd, &aux, sizeof(char));
		write(fd, header, sizeof(char)*strlen(header));

		if (bytes_left <= data_length+1) {

			write(fd, new_data, sizeof(char)*strlen(new_data));
			aux = '\0';
			for (int i=strlen(new_data); i<data_length; i++) {
				write(fd, &aux, sizeof(char));
			}
		}
		else {
			write(fd, new_data, sizeof(char)*data_length);
		}

		bytes_left = bytes_left - data_length;
		if (bytes_left > 0)	{
			new_data = new_data + data_length;
		}
		log("Packet sent: "); debug(header); debug(data);
	} while(bytes_left > 0);
}


void send_file_packet(int fd, char* id, char*data) {

	char* pack = (char*) calloc(257, sizeof(char));

	pack[0] = (char) 4;
	pack[1] = (char) 0;
	pack[2] = (char) strlen("FILE_DATA");
	pack[3] = 'F';
	pack[4] = 'I';
	pack[5] = 'L';
	pack[6] = 'E';
	pack[7] = '_';
	pack[8] = 'D';
	pack[9] = 'A';
	pack[10] = 'T';
	pack[11] = 'A';
	pack[12] = id[0];
	pack[13] = id[1];
	pack[14] = '&';
	pack[15] = '\0';
	
	int j=0;
	for (int i=15; i<256; i++) {
		pack[i] = data[j];
		j++;
	}
	pack[256] = '\0';

	write(fd, pack, 256*sizeof(char));
	free(pack);
}

void remove_symbol(char* string, char symbol){
	int i1 = 0, i2 = 0;
	while (string[i2] != '\0'){
        
        if (string[i2] != symbol){
            string[i1] = string[i2];
            i1++;
        }
        i2++;
    }
    string[i1] = '\0';
}

char* get_md5sum(char* path) {
	
	char* buffer = (char*) calloc(33, sizeof(char));

	int pipe_fd[2];
	if (pipe(pipe_fd)==-1) {
		return "";
	}
	
	int child = fork();
	if (child > 0){ //parent

		close(pipe_fd[1]);
        read(pipe_fd[0], buffer, 32);
		buffer[32] = '\0';
        close(pipe_fd[0]);
	}
	else if (child == 0){ //child

		close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);

        char *command[] = {"md5sum", path, NULL};
        if (execvp(command[0], command) == -1) {
			write(pipe_fd[1], "01234567890123456789012345678901", 32);
        }
		close(pipe_fd[1]);
	}
	else {
		return "";
	}
	wait(NULL);

	return buffer;
}