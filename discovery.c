#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "commonfuncs.h"

#define ERR_ARGC "There should be exactly 1 argument."
#define ERR_FD "Configuration file not found"

#define ERR -1

typedef struct {
    char* ip_poole;
    int port_poole;
    char* ip_bowman;
    int port_bowman;
} Config;

Config config;

int main(int argc, char** argv) {

    int fd;
    const char* file_path_start = "config_files/";
    char* file_path = NULL;
    char* buffer = NULL;

    if (argc != 2) {
        logn(ERR_ARGC);
        return ERR;
    }

    file_path = (char*) malloc(sizeof(char) * (strlen(file_path_start) + strlen(argv[1]) - 1));
    strcat(file_path, file_path_start);
    strcat(file_path, argv[1]);

    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        logn(ERR_FD);
        return ERR;
    }

    config.ip_poole = read_until(fd, '\n');
    buffer = read_until(fd, '\n');
    config.port_poole = atoi(buffer);
    buffer[0] = '\0';
    config.ip_bowman = read_until(fd, '\n');
    buffer = read_until(fd, '\n');
    config.port_bowman = atoi(buffer);


    logn(config.ip_poole);
    logni(config.port_poole);
    logn(config.ip_bowman);
    logni(config.port_bowman);

    free(config.ip_poole);
    free(config.ip_bowman);
    free(file_path);
    free(buffer);
    close(fd);

}