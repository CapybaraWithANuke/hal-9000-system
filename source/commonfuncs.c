#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define log(x) write(1, x, strlen(x))

typedef struct {
    char type;
    char* header;
    char* data;
} Packet;

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

char** split(char* string, char character){
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

void logni(int x) {
    char* buffer;
    asprintf(&buffer, "%d\n", x);
    log(buffer);
    free(buffer);
}

Packet read_packet(int fd) {
	Packet new_packet;
	short header_length;
	short data_length;
	short i;

	read(fd, &new_packet.type, sizeof(char));
	read(fd, &header_length, sizeof(short));

	new_packet.header = (char*) malloc(sizeof(char)*(header_length+1));
	data_length = 256 - 3 - header_length;

	for (i=0; i<header_length; i++){
		read(fd, &new_packet.header[i], sizeof(char));
	}
	new_packet.header[i] = '\0';

	for (i=0; i<data_length+1; i++){
		read(fd, &new_packet.data[i], sizeof(char));
	}
	new_packet.data[i] = '\0';

	return new_packet;
} 

char* buildFrame(char type, int header_length, char* header, char* data) {

	char* frame = (char*) calloc(1, sizeof(char));

	strcat(frame, type);

	char* length = (header_length / 10 == 0) ? ((char*) malloc(sizeof(char) * 2)) : ((char*) malloc(sizeof(char) * 3));
	asprintf(&length, "%d", header_length);
	
	strcat(frame, length);

	free(length);

	strcat(frame, header);
	strcat(frame, data);

	return frame;

}