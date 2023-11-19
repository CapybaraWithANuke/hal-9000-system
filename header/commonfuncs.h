#ifndef COMMONFUNCS_H
#define COMMONFUNCS_H

#define log(x) write(1, x, strlen(x))

typedef struct {
    char type;
    char* header;
    char* data;
} Packet;


char * read_until(int fd, char end);
void logn(char* x);
void logni(int x);
Packet read_packet(int fd);
char* buildFrame(char type, int header_length, char* header, char* data);
char** split(char* string, char character);

#endif