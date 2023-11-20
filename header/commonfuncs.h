#ifndef COMMONFUNCS_H
#define COMMONFUNCS_H

#define log(x) write(1, x, strlen(x))

typedef struct {
    char type;
    char empty;
    int header_length;
    char* header;
    char* data;
} Packet;


char * read_until(int fd, char end);
void logn(char* x);
void logni(int x);
Packet read_packet(int fd);
void debug(char* x);
void fill_with(char symbol, char* data, int size);
void send_packet(int fd, int type, char* header, char*data);
char** split(char* string, char character);
void remove_symbol(char* string, char symbol);

#endif