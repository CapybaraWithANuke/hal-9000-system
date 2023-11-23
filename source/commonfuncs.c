#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../header/commonfuncs.h"

char * read_until(int fd, char end) {
	char *string = NULL;
	char c;
	int i = 0, size;

	while (1) {
		size = read(fd, &c, sizeof(char));
		if(string == NULL){
			string = (char *) malloc(sizeof(char));
		}
		if(c != end && size > 0){
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

char** split(char* string, char character) {
	char** strings;
	int i=0, j, k=0;

	do {
		strings = (char**) realloc(strings, sizeof(char*)*(i + 1));
		strings[i] = (char*) malloc(sizeof(char));
		for (j=0; string[k] != character && string[k] != '\0'; k++){

			strings[i][j] = string[k];
			strings[i] = (char*) realloc(strings[i], sizeof(char)*(j + 2));
			j++;
		}
		strings[i][j] = '\0';
		i++;
		k++;
	} while (string[k-1] != '\0');

	return strings;
}

void logn(char* x) {
	char* x_new = (char*) calloc(1, strlen(x)+1);
    strcat(x_new, x);
    strcat(x_new, "\n");
    log(x_new);
    free(x_new);
}

void debug(char* x) {
	char* x_new = (char*) calloc(1, strlen(x)+1);
    strcat(x_new, x);
    strcat(x_new, "\n");
    log(x_new);
    free(x_new);
}

void logni(int x) {
    char* buffer;
    asprintf(&buffer, "%d\n", x);
    log(buffer);
    free(buffer);
}

Packet read_packet(int fd) {
	Packet new_packet;
	short data_length;
	short i;

	read(fd, &new_packet.type, sizeof(char));
	read(fd, &new_packet.empty, sizeof(char));
	read(fd, &new_packet.empty, sizeof(char));
	new_packet.header_length = (int) new_packet.empty;

	new_packet.header = (char*) malloc(sizeof(char)*(new_packet.header_length));
	data_length = 256 - 3 - new_packet.header_length;

	for (i=0; i<new_packet.header_length; i++){
		read(fd, &new_packet.header[i], sizeof(char));
	}
	new_packet.header[i] = '\0';

	for (i=0; i<data_length; i++){
		read(fd, &new_packet.data[i], sizeof(char));
	}
	new_packet.data[i-1] = '\0';

	debug(new_packet.header);
	debug(new_packet.data);

	return new_packet;

}

void send_packet(int fd, int type, char* header, char*data) {

    Packet packet;
	char aux;

    packet.type = (char) type;
    packet.header_length = strlen(header)+1;

    packet.header = (char*) malloc(sizeof(char)*(packet.header_length+1));
	strcpy(packet.header, header);

    int data_length = 256 - 3 - packet.header_length;
    packet.data = (char*) malloc(sizeof(char)*data_length);
    fill_with('\0', packet.data, data_length);
	strcpy(packet.data, data);

	write(fd, packet.data, sizeof(char));
	aux = (char) 0;
	write(fd, &aux, sizeof(char));
	aux = (char) packet.header_length;
	write(fd, &aux, sizeof(char));
	write(fd, packet.header, sizeof(char)*packet.header_length);
	write(fd, packet.data, sizeof(char)*data_length);

	free(packet.header);
	free(packet.data);
}

void fill_with(char symbol, char* data, int size) {

	for (int i=0; i<size; i++){
		data[i] = symbol;
	}
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