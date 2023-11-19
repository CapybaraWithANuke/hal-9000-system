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
char** split(char* string, char character);
Packet read_packet(int fd);
void debug(char* x);

#endif