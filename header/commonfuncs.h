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
char * read_until2(int fd, char end1, char end2);
void logn(char* x);
void debug(char* x);
void logi(int x);
void logni(int x);
void fill_with(char symbol, char* data, int size);
Packet read_packet(int fd);
void send_packet(int fd, int type, char* header, char*data);
void remove_symbol(char* string, char symbol);
char* get_md5sum(char* path);

#endif