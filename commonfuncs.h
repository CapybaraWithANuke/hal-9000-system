#ifndef COMMONFUNCS_H
#define COMMONFUNCS_H

#define log(x) write(1, x, strlen(x))

char * read_until(int fd, char end);
void logn(char* x);
void logni(int x);

#endif