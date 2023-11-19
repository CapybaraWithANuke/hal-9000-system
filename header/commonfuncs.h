#ifndef COMMONFUNCS_H
#define COMMONFUNCS_H

#define log(x) write(1, x, strlen(x))

typedef struct {
    char type;
    short header_length;
    char* header;
    char* data;
} Packet;

char * read_until(int fd, char end);
void logn(char* x);
void logni(int x);
char** split(char* string, char character);
Packet read_packet(int fd);
void debug(char* x);
void fil_with(char symbol, char* data, int size);
void send_packet(int fd, int type, char* header, char*data);

#endif