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
	char* buffer;
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