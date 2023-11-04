#include <stdio.h>
#include <string.h> // strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>    // close
#include <arpa/inet.h> // close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> // FD_SET, FD_ISSET, FD_ZERO macros
#include <stdbool.h>
#include <wordexp.h>
#include <dirent.h>

#include "utils.h"
#include "djb2.h"

#define WELCOME_MSG "Welcome to file transfer server\n"
#define DATA_FOLDER "data"
#define DATA_FOLDER_LEN 4
#define MAX_CLIENTS 30

void list_dir(DIR* d, int sd, char* buffer) {
    struct dirent *dir = NULL;
    buffer[0] = 0;
    while ((dir = readdir(d)) != NULL)
        if (dir->d_type == DT_REG)
            strcat(buffer, strcat(dir->d_name, "\n"));
    rewinddir(d);
    send_wrapper(sd, buffer, 0);
}

void send_file(char* name, char* path, char* buffer, int sd) {
    bool result = false;
    if (strcmp(name, "..") == 0 || strcmp(name, ".") == 0) {
        send(sd, &result, sizeof(bool), 0);
        send_wrapper(sd, "Invalid filename\n", 0);
        return;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        send(sd, &result, sizeof(bool), 0);
        send_wrapper(sd, "File not found\n", 0);
    } else {
        result = 1;
        send(sd, &result, sizeof(bool), 0);
        send_wrapper(sd, name, 0);
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        rewind(fp);
        send(sd, &size, sizeof(size_t), 0);
        size_t length = 0;
        while ((length = fread(buffer, 1, BSIZE, fp)) > 0) {
            send(sd, buffer, length, 0);
        }
        fclose(fp);
    }
}

void get(char* filename, char* path_buffer, char* data_buffer, int sd) {
    int length = DATA_FOLDER_LEN + strlen(filename) + 2;
    if (path_buffer)
        path_buffer = realloc(path_buffer, length * sizeof(char));
    else
        path_buffer = malloc(length * sizeof(char));
    snprintf(path_buffer, length, "%s/%s", DATA_FOLDER, filename);
    send_file(filename, path_buffer, data_buffer, sd);
}

int main() {
    int opt = true;
    int master_socket, addrlen, new_socket, activity, i, valread, sd;
    int client_socket[MAX_CLIENTS];
    int max_sd, portno, req_ret;
    struct sockaddr_in address;
    char *filename = NULL;
    wordexp_t args_request;
    struct dirent *dir = NULL;
    char buffer[BSIZE + 1];
    DIR* d;

    // set of socket descriptors
    fd_set readfds;

    for (int i = 0; i < MAX_CLIENTS; i++)
        client_socket[i] = 0;
    
    d = opendir(DATA_FOLDER);
    if (d == NULL) {
        fprintf(stderr, "Could not open data folder\n");
        exit(EXIT_FAILURE);
    }

    // create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        serr("socket failed\n");

    // set master socket to allow multiple connections ,
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        serr("setsockopt failed\n");

    // type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost `PORT`
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        serr("bind failed\n");

    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
        serr("listen failed\n");

    // accept the incoming connection
    addrlen = sizeof(address);

    while (true)
    {
        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // add child sockets to set
        for (i = 0; i < MAX_CLIENTS; i++) {
            // socket descriptor
            sd = client_socket[i];

            // if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        // wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
            fprintf(stderr, "select error");

        // If something happened on the master socket, then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
                serr("accept failed\n");

            // inform user of socket number - used in send and receive commands
            printf("INFO: New connection , socket fd is %d , ip is : %s , port : %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            send_wrapper(new_socket, WELCOME_MSG, 0);

            // add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                // if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // else its some IO operation on some other socket
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readfds)) {
                portno = htons(address.sin_port);
                // Check if it was for closing , and also read the incoming message
                if ((valread = read(sd, buffer, BSIZE)) == 0)
                {
                    // Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("INFO: Host disconnected , ip %s , port %d\n", inet_ntoa(address.sin_addr), portno);
                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    for (int i = 0; i < valread; ++i) {
                        if (buffer[i] == '\n' || buffer[i] == '\r') {
                            buffer[i] = '\0';
                            break;
                        }
                    }
                    buffer[valread] = '\0';
                    req_ret = wordexp(buffer, &args_request, 0);
                    printf("Request: %s\n", buffer);
                    if (req_ret == 0) {
                        if (args_request.we_wordc == 0) {
                            send_wrapper(sd, "Specify a command\n", 0);
                            continue;
                        }
                        switch (hash(args_request.we_wordv[0])) {
                            case HASH_LS:
                                list_dir(d, sd, buffer);
                                break;
                            case HASH_GET:
                                if (args_request.we_wordc < 2) {
                                    bool result = false;
                                    send(sd, &result, sizeof(bool), 0);
                                    send_wrapper(sd, "Specify a filename\n", 0);
                                }
                                else
                                    get(args_request.we_wordv[1], filename, buffer, sd);
                                break;
                            case HASH_EXIT:
                                send_wrapper(sd, "Goodbye\n", 0);
                            default:
                                send_wrapper(sd, "Unknown command\n", 0);
                                break;
                        }
                    } else {
                        fprintf(stderr, "wordexp failed with code %d\n", req_ret);
                    }
                    
                }
            }
        }
    }
    wordfree(&args_request);
    free(filename);
    free(dir);
    free(d);
    close(master_socket);
    return 0;
}
