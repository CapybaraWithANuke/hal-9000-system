#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <math.h>
#include <pthread.h>

#include "../header/commonfuncs.h"
#include "../header/semaphore_v2.h"

#define ERR_SOCKET "Error: socket not created"
#define ERR_CONNECT "Error: connection not established"
#define ERR_BIND "Error: binding failed"
#define ERR_LISTEN "Error: listening failed"
#define ERR_ACCEPT "Error: accepting failed"

#define ERR_PID "Error: fork not created"

#define ERR_PIPE "Error: pipe not created"

#define ERR_STATS "Error: stats file not opened"

#define ERR -1

#define PLAYLIST_COUNT 3
#define SONG_COUNT 5
#define MAX_SONG_LENGTH 256


typedef struct{
	char* server_name; 
	char* directory;
	char* discovery_ip;
	int discovery_port;
    char* poole_ip;
	int poole_port;
} Poole;

typedef struct {
    char* name;
    int num_songs;
    char** songs;
} Playlist;

typedef struct {
    char* name;
    int num_downloads;
} Song;

typedef struct {
    int fd;
    char* song;
    int thread_num;
} Thread_input;

Poole poole;
semaphore print_sem;
semaphore thread_array_sem;
semaphore packet_sem;
semaphore stats_file;
semaphore write_pipe;
semaphore monolith_ready;
pthread_t* threads;
int* thread_fin;
Thread_input* inputs;
int num_threads = 0;
int pip[2];

void free_everything(){
    free(poole.server_name);
    free(poole.directory);
    free(poole.discovery_ip);
    free(poole.poole_ip);
}

int setup_socket(char* ip, int port, int server_fd) {

    struct sockaddr_in address;
    socklen_t addrlen;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);
    addrlen = sizeof(address);

    if (bind(server_fd, (struct sockaddr*)&address, addrlen) < 0) {
        logn(ERR_BIND);
        return ERR;
    }

    if (listen(server_fd, 5) < 0) {
        logn(ERR_LISTEN);
        return ERR;
    }

    return 1;
}

void protected_logn(semaphore* sem, char* x) {
	SEM_wait(sem);
	logn(x);
	SEM_signal(sem);
}

void send_ppacket(int fd, int type, char* header, char*data) {
    SEM_wait(&packet_sem);
    send_packet(fd, type, header, data);
    SEM_signal(&packet_sem);
}

void list_songs(int fd) {

    int ffd = open("poole_music/song_names", O_RDONLY);
    if (ffd < 0){
        perror("Error: unable to get songs\n");
        send_ppacket(fd, 2, "SONGS_RESPONSE", "");
        return;
    }

    char* songs = (char*) calloc(1, sizeof(char));
    songs[0] = '\0';
    char* song_name = (char*) calloc(1, sizeof(char));

    do {
        free(song_name);
        song_name = read_until(ffd, ' ');
        if (song_name[0] == '\0') {
            free(song_name);
            break;
        }
        free(song_name);
        song_name = read_until(ffd, '\n');
        remove_symbol(song_name, '\r');
        songs = (char*) realloc(songs, (strlen(songs) + 2 + strlen(song_name))*sizeof(char));

        if (songs[0] != '\0') {
            strcat(songs, "&");
        }
        strcat(songs, song_name);
    } while(song_name[0] != '\0');

    send_ppacket(fd, 2, "SONGS_RESPONSE", songs);
    free(songs);
    close(ffd);
}

void list_playlists(int fd) {

    int ffd = open("poole_music/playlists", O_RDONLY);
    if (ffd < 0){
        perror("Error: unable to get playlists\n");
    }

    char* string = (char*) calloc(1, sizeof(char));
    string[0] = '\0';
    char* buffer;

    do {
        buffer = read_until(ffd, '-');
        if (buffer[0] == '\0') {
            break;
        }
        string = (char*) realloc(string, (strlen(string) + 2 + strlen(buffer))*sizeof(char));
        
        if (string[0] != '\0') {
            strcat(string, "#");
        }
        strcat(string, buffer);

        do {
            free(buffer);
            buffer = read_until2(ffd, ' ', '\n');
            remove_symbol(buffer, '\r');
            if (buffer[0] == '\0') {
                break;
            }
            free(buffer);
            buffer = read_until(ffd, ',');

            string = (char*) realloc(string, (strlen(string) + 2 + strlen(buffer))*sizeof(char));
            strcat(string, "&");
            strcat(string, buffer);
        } while(1);
        free(buffer);
    } while(1);

    send_ppacket(fd, 2, "SONGS_RESPONSE", string);
    free(string);
    close(ffd);
}

void signal_thread_end(int thread_num) {
    SEM_wait(&thread_array_sem);
    thread_fin[thread_num] = 1;
    SEM_signal(&thread_array_sem);
}

void* send_song(void* in) {

    Thread_input* input_p = (Thread_input*) in;
    int fd = input_p->fd;
    int thread_num = input_p->thread_num;
    char* song = input_p->song;
    char* path = (char*) calloc(strlen("poole_music/song_names") + 1, sizeof(char));
    strcpy(path, "poole_music/song_names");

    char* song_name = (char*) calloc(strlen(song) + 1, sizeof(char));
    strcpy(song_name, song);
    song_name[strlen(song_name) - 4] = '\0';

    SEM_wait(&monolith_ready);
    write(pip[1], song_name, strlen(song_name) + 1);
    SEM_signal(&write_pipe);

    free(path);
    path = (char*) calloc(strlen("poole_music/song_names") + 1, sizeof(char));
    strcpy(path, "poole_music/song_names");

    int songs_ffd = open(path, O_RDONLY);
    if (songs_ffd == -1) {
        signal_thread_end(thread_num);
        logn("error opening file");
        free(path);
        return NULL;
    }

    char* id = NULL;
    char* buffer = NULL;
    int found = 0;

    do {
        free(id);
        free(buffer);

        id = read_until(songs_ffd, ' ');
        buffer = read_until(songs_ffd, '\n');
        remove_symbol(buffer, '\r');

        if (strcmp(buffer, song) == 0) {
            found = 1;
            break;
        }
    } while(buffer && buffer[0] != '\0');

    close(songs_ffd);

    if (!found) {
        free(buffer);
        free(id);
        signal_thread_end(thread_num);
        return NULL;
    }

    path = (char*) realloc(path, (strlen("poole_music/song_files/") + strlen(song) + 1) * sizeof(char));
    strcpy(path, "poole_music/song_files/");
    strcat(path, song);
    free(song);

    char* md5sum = get_md5sum(path);

    struct stat file_info;
    if (stat(path, &file_info) == -1) {
        signal_thread_end(thread_num);
        free(path);
        return NULL;
    }

    char* filesize;
    asprintf(&filesize, "%ld", file_info.st_size);

    int id_int = atoi(id);
    char* id_conv = (char*) calloc(3, sizeof(char));
    id_conv[0] = id_int / 256;
    id_conv[1] = id_int % 256;
    id_conv[2] = '\0';

    buffer = (char*) realloc(buffer, (strlen(buffer) + strlen(filesize) + strlen(md5sum) + strlen(id) + 4) * sizeof(char));
    strcat(buffer, "&");
    strcat(buffer, filesize);
    strcat(buffer, "&");
    strcat(buffer, md5sum);
    strcat(buffer, "&");
    strcat(buffer, id);
    send_ppacket(fd, 4, "NEW_FILE", buffer);

    free(buffer);
    free(md5sum);
    free(filesize);
    free(id);

    int ffd = open(path, O_RDONLY);
    free(path);
    if (ffd == -1) {
        free(id_conv);
        send_ppacket(fd, 4, "ERROR_FILE", "");
        signal_thread_end(thread_num);
        return NULL;
    }

    int data_length = 256 - 3 - 3 - strlen("FILE_DATA");
    buffer = (char*) calloc(data_length + 1, sizeof(char));

    int size_read, i, j;
    for (i = 0; i < (int)file_info.st_size; i += data_length) {
        size_read = read(ffd, buffer, sizeof(char) * data_length);
        for (j = size_read; j < data_length; j++) {
            buffer[j] = '\0';
        }
        buffer[data_length] = '\0';
        SEM_wait(&packet_sem);
        send_file_packet(fd, id_conv, buffer);
        SEM_signal(&packet_sem);
    }

    debug("finished sending song");

    free(buffer);
    free(id_conv);
    signal_thread_end(thread_num);
    return NULL;

}

void send_playlist(int fd, char* playlist) {

    char* path = "poole_music/playlists";
    int playlist_ffd = open(path, O_RDONLY);
    if (playlist_ffd == -1){
        return;
    }

    int num_songs = 0;
    char* playlist_file = (char*) calloc(1, sizeof(char));
    playlist_file[0] = '\0';
    char* buffer;
    char** songs = (char**) calloc(1, sizeof(char*));

    do {
        free(playlist_file);
        playlist_file = read_until(playlist_ffd, '-');
        if (playlist_file[0] == '\0') {
            break;
        }

        do {
            buffer = read_until2(playlist_ffd, ' ', '\n');
            remove_symbol(buffer, '\r');
            remove_symbol(buffer, '\n');
            if (buffer[0] == '\0') {
                free(buffer);
                break;
            }
            free(buffer);
            buffer = read_until(playlist_ffd, ',');
            remove_symbol(buffer, '\r');
            remove_symbol(buffer, '\n');
            
            if (!strcmp(playlist, playlist_file)) {
                songs = (char**) realloc(songs, ++num_songs*sizeof(char*));
                songs[num_songs-1] = buffer;
            }
            else {
                free(buffer);
            }
        } while(1);
    } while(strcmp(playlist, playlist_file));
    free(playlist_file);

    log(". A total of "); logi(num_songs); logn(" songs will be sent.");
    SEM_signal(&print_sem);

    SEM_wait(&thread_array_sem);
    inputs = (Thread_input*) realloc(inputs, (num_threads+num_songs)*sizeof(Thread_input));
    threads = (pthread_t*) realloc(threads, (num_threads+num_songs)*sizeof(pthread_t));
    thread_fin = (int*) realloc(thread_fin, (num_threads+num_songs)*sizeof(int));

    for (int i=0; i<num_songs; i++) {
        
        inputs[num_threads+i].fd = fd;
        inputs[num_threads+i].song = (char*) calloc(strlen(songs[i])+1, sizeof(char));
        strcpy(inputs[num_threads+i].song, songs[i]);
        inputs[num_threads+i].thread_num = num_threads + i;
        thread_fin[num_threads+i] = 0;
        pthread_create(&threads[num_threads+i], NULL, send_song, (void*) &inputs[num_threads+i]);
    }
    SEM_signal(&thread_array_sem);
    return;
}

void monolith(int fd_pipe) {

    char* song_name;

    while(1) {

        SEM_signal(&monolith_ready);
        SEM_wait(&write_pipe);
        song_name = read_until(fd_pipe, '\0');
        logn(song_name);

        SEM_wait(&stats_file);
        int fd = open("stats.txt", O_RDONLY);

        if (fd < 0) {
            logn(ERR_STATS);
            exit(ERR);
        }

        Song* songs = (Song*) malloc(1*sizeof(Song));
        char* current_word = "a";
        int i = 0;

        while (strcmp(current_word, "\0")) {
            
            songs = (Song*) realloc(songs, (i+1)*sizeof(Song));
            current_word = read_until(fd, ',');
            songs[i].name = (char*) malloc(strlen(current_word)*sizeof(char));
            strcpy(songs[i].name, current_word);
            current_word = read_until(fd, '\n');
            songs[i].num_downloads = atoi(current_word);

            if (!strcmp(songs[i].name, song_name)) {
                ++songs[i].num_downloads;
            }

            ++i;
        }

        close(fd);

        fd = open("stats.txt", O_TRUNC | O_WRONLY);

        for (int j = 0; j < (i-1); ++j) {

            write(fd, songs[j].name, strlen(songs[j].name));
            write(fd, ",", 1);
            char* num_downloads_string;
            asprintf(&num_downloads_string, "%d", songs[j].num_downloads);
            write(fd, num_downloads_string, strlen(num_downloads_string));
            write(fd, "\n", 1);

        }
        
        close(fd);
        SEM_signal(&stats_file);

    }
    free(song_name);
    close(fd_pipe);
    return;
}

int main(int argc, char *argv[]) {
	
    char* string;
    char* file_path = NULL;
    int discovery_fd = -1;
    struct sockaddr_in serv_addr;

    if (argc != 2){
        logn("Error: There must be exactly one parameter when running");
        return 0;
    }

    file_path = (char*) calloc((strlen("config/") + strlen(argv[1]) + 1), sizeof(char));
    strcat(file_path, "config/");
    strcat(file_path, argv[1]);

    int fd_config = open(file_path, O_RDONLY);
    if (fd_config == -1){
        perror("Error: unable to open file\n");
        exit(EXIT_FAILURE);
    }

	poole.server_name = read_until(fd_config, '\n');
    remove_symbol(poole.server_name, '\r');
	poole.directory = read_until(fd_config, '\n');
    remove_symbol(poole.directory, '\r');
	poole.discovery_ip = read_until(fd_config, '\n');
    remove_symbol(poole.discovery_ip, '\r');
	string = read_until(fd_config, '\n');
    remove_symbol(string, '\r');
	poole.discovery_port = atoi(string);
    free(string);
    poole.poole_ip = read_until(fd_config, '\n');
    remove_symbol(poole.poole_ip, '\r');
    string = read_until(fd_config, '\n');
    remove_symbol(string, '\r');
	poole.poole_port = atoi(string);
    free(string);
    close(fd_config);

    threads = calloc(1, sizeof(pthread_t));
    thread_fin = calloc(1, sizeof(int));
    inputs = calloc(1, sizeof(Thread_input));

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(poole.discovery_port);
    if(inet_pton(AF_INET, poole.discovery_ip, &serv_addr.sin_addr) < 0) {
        logn("Error connecting to the server\n");
        return ERR;
    }
    if ((discovery_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        logn("Error connecting to the server\n");
        return ERR;
    }
    if (connect(discovery_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logn(ERR_CONNECT);
        return ERR;
    }
    char* buffer;
    asprintf(&buffer, "%d", poole.poole_port);

    SEM_constructor(&write_pipe);
    SEM_init(&write_pipe, 0);
    SEM_constructor(&monolith_ready);
    SEM_init(&monolith_ready, 0);

    if (pipe(pip) < 0) {
        logn(ERR_PIPE);
        return ERR;
    }

    pid_t pid = fork();

    if (pid < 0) {
        logn(ERR_PID);
        return ERR;
    }

    if (pid == 0) {
        close(pip[1]);
        SEM_constructor_with_name(&stats_file, ftok("stats.txt", 1));
        SEM_init(&stats_file, 1);
        monolith(pip[0]);
        exit (-1);
    }
    else {
        close(pip[0]);
        // char* buffer;
        // asprintf(&buffer, "ingobernable");
        // write(pip[1], buffer, strlen(buffer)+1);
        // free(buffer);
        
    }

    char* frame_data = calloc(strlen(poole.server_name)+strlen(poole.poole_ip)+strlen(buffer)+3, sizeof(char));
    strcat(frame_data, poole.server_name);
    strcat(frame_data, "&");
    strcat(frame_data, poole.poole_ip);
    strcat(frame_data, "&");
    strcat(frame_data, buffer);

    send_packet(discovery_fd, 1, "NEW_POOLE", frame_data);

    free(buffer);
    free(frame_data);

    Packet packet = read_packet(discovery_fd);

    while (!strcmp(packet.header, "CON_KO")) {
        char* frame_data = calloc(strlen(poole.server_name)+strlen(poole.poole_ip)+7, sizeof(char));
        strcat(frame_data, poole.server_name);
        strcat(frame_data, "&");
        strcat(frame_data, poole.poole_ip);
        strcat(frame_data, "&");
        char* buffer = calloc(5, sizeof(char));
        sprintf(buffer, "%d", poole.poole_port);
        strcat(frame_data, buffer);
        free(buffer);
        
        send_packet(discovery_fd, 1, "NEW_POOLE", frame_data);

        free(frame_data);

        packet = read_packet(discovery_fd);
    }
    free(packet.data);
    free(packet.header);

    int i = 0;
    do {
        if (i != 0){
            free(packet.header);
            free(packet.data);
        }
        send_packet(discovery_fd, 6, "EXIT_POOLE", "");
        packet = read_packet(discovery_fd);
        i++;
    } while (strcmp(packet.header, "CON_OK"));

    close(discovery_fd);
    free(packet.data);
    free(packet.header);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_fd < 0) {
        logn(ERR_SOCKET);
        return ERR;
    }

    struct sockaddr_in address;
    socklen_t addrlen;

    address.sin_family = AF_INET;
    address.sin_port = htons(poole.poole_port);
    address.sin_addr.s_addr = inet_addr(poole.poole_ip);
    addrlen = sizeof(address);

    fd_set set;
    FD_ZERO(&set);
    FD_SET(server_fd, &set);
    int* sockets_fd = (int*) calloc(1, sizeof(int));
    char** names_bowmans = (char**) calloc(1, sizeof(char*));
    int sockets_index = 0;
    int nfds = server_fd+1;

    setup_socket(poole.poole_ip, poole.poole_port, server_fd);
    struct timeval timeout = {0, 1};

    SEM_constructor(&print_sem);
    SEM_constructor(&thread_array_sem);
    SEM_constructor(&packet_sem);
    SEM_init(&print_sem, 1);
    SEM_init(&thread_array_sem, 1);
    SEM_init(&packet_sem, 1);

    while(1) {

        FD_ZERO(&set);
        FD_SET(server_fd, &set);
        for (i = 0; i < sockets_index; i++) {
            FD_SET(sockets_fd[i], &set);

            if (sockets_fd[i] + 1 > nfds) {
                nfds = sockets_fd[i] + 1;
            }
        }
        select(nfds, &set, NULL, NULL, &timeout);

        if (FD_ISSET(server_fd, &set)) {

            if ((sockets_fd[sockets_index] = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
                logn(ERR_ACCEPT);
                return ERR;
            }
            FD_SET(sockets_fd[sockets_index], &set);
            ++sockets_index;
            sockets_fd = (int*) realloc(sockets_fd, sizeof(int)*(sockets_index + 1));
            names_bowmans = (char**) realloc(names_bowmans, sizeof(char*)*(sockets_index + 1));
        }

        for (i = 0; i < sockets_index; ++i) {

            if (FD_ISSET(sockets_fd[i], &set)) {
                
                packet = read_packet(sockets_fd[i]);

                switch((int) packet.type) {

                    case 1:
                        names_bowmans[i] = (char*) calloc(strlen(packet.data)+1, sizeof(char));
                        strcpy(names_bowmans[i], packet.data);

                        send_ppacket(sockets_fd[i], 1, "CON_OK", "");
                        break;
                    case 2:
                        if (!strcmp(packet.header, "LIST_SONGS")) {
                            SEM_wait(&print_sem);
                            logn("New request - "); log(names_bowmans[i]); logn(" requires the list of songs.");
                            log("Sending song list to "); logn(names_bowmans[i]);
                            SEM_signal(&print_sem);
                            list_songs(sockets_fd[i]);
                        }
                        else if (!strcmp(packet.header, "LIST_PLAYLISTS")) {
                            SEM_wait(&print_sem);
                            logn("New request - "); log(names_bowmans[i]); logn(" requires the list of playlists.");
                            log("Sending playlist list to "); logn(names_bowmans[i]);
                            SEM_signal(&print_sem);
                            list_playlists(sockets_fd[i]);
                        }
                        break;
                    case 3:
                        if (!strcmp(packet.header, "DOWNLOAD_SONG")) {
                            SEM_wait(&print_sem);
                            logn("New request - "); log(names_bowmans[i]); log(" want to download "); logn(packet.data);
                            log("Sending "); log(packet.data); log(" to "); logn(names_bowmans[i]);
                            SEM_signal(&print_sem);

                            SEM_wait(&thread_array_sem);
                            inputs = (Thread_input*) realloc(inputs, ++num_threads*sizeof(Thread_input));
                            inputs[num_threads-1].fd = sockets_fd[i];
                            inputs[num_threads-1].song = (char*) calloc(strlen(packet.data)+1, sizeof(char));
                            strcpy(inputs[num_threads-1].song, packet.data);
                            inputs[num_threads-1].thread_num = num_threads - 1;

                            threads = (pthread_t*) realloc(threads, num_threads*sizeof(pthread_t));
                            thread_fin = (int*) realloc(thread_fin, num_threads*sizeof(int));
                            thread_fin[num_threads-1] = 0;
                            pthread_create(&threads[num_threads-1], NULL, send_song, (void*) &inputs[num_threads-1]);
                            SEM_signal(&thread_array_sem);
                        }
                        else if (!strcmp(packet.header, "DOWNLOAD_LIST")) {
                            SEM_wait(&print_sem);
                            logn("New request - "); log(names_bowmans[i]); log(" want to download the playlist "); logn(packet.data);
                            log("Sending "); log(packet.data); log(" to "); logn(names_bowmans[i]);
                            send_playlist(sockets_fd[i], packet.data);
                        }
                        break;
                    case 6:
                        if (!strcmp(packet.header, "EXIT")) {
                            send_ppacket(sockets_fd[i], 6, "CONOK", "");
                            close(sockets_fd[i]);
                            
                            int k=0;
                            for (int j=0; j<sockets_index-k; j++) {
                                if (i == j){
                                    k++;
                                }
                                sockets_fd[j] = sockets_fd[j+k];
                            }
                            sockets_index = sockets_index - k;
                        }
                        break;
                    default:
                        break;
                }
                free(packet.header);
                free(packet.data);
                  
            }
        }

        SEM_wait(&thread_array_sem);
        int j = 0;
        for (i=0; i<num_threads-j; i++) {

            if (thread_fin[i] == 1) {
                pthread_join (threads[i], NULL);
                j++;
            }
            if (j > 0) {
                threads[i] = threads[i+j];
                thread_fin[i] = thread_fin[i+j];
                inputs[i] = inputs[i+j];
            }
        }
        if (j > 0) {
            num_threads = num_threads - j;
            if (num_threads > 0) {
                inputs = (Thread_input*) realloc(inputs, (num_threads)*sizeof(Thread_input));
                threads = (pthread_t*) realloc(threads, (num_threads)*sizeof(pthread_t));
                thread_fin = (int*) realloc(thread_fin, (num_threads)*sizeof(int));
            }
            else {
                inputs = (Thread_input*) calloc(1, sizeof(Thread_input));
                threads = (pthread_t*) calloc(1, sizeof(pthread_t));
                thread_fin = (int*) calloc(1, sizeof(int));
            }
        }
        SEM_signal(&thread_array_sem);
    }

    free_everything();
	
    return 0;
}