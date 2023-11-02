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

#include "djb2.c"

#define PORT 8080
#define WELCOME_MSG "Welcome to file transfer server\n"
#define DATA_FOLDER "data/"
#define BSIZE 4096
#define MAX_CLIENTS 30

int main(int argc, char *argv[]) {
    int opt = true;
    int master_socket, addrlen, new_socket, activity, i, valread, sd;
    int client_socket[MAX_CLIENTS];
    DIR* sessions[MAX_CLIENTS];
    int max_sd, portno, req_ret;
    struct sockaddr_in address;
    char *filename, *filepath = NULL, *workdirs[MAX_CLIENTS];
    wordexp_t args_request;
    struct dirent *dir;
    char buffer[BSIZE + 1];

    // set of socket descriptors
    fd_set readfds;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        sessions[i] = NULL;
        client_socket[i] = 0;
        workdirs[i] = NULL;
    }

    // create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost `PORT`
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

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

        // wait for an activity on one of the sockets , timeout is NULL ,
        // so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
            fprintf(stderr, "select error");

        // If something happened on the master socket ,
        // then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // inform user of socket number - used in send and receive commands
            printf("INFO: New connection , socket fd is %d , ip is : %s , port : %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // send new connection greeting message
            if (send(new_socket, WELCOME_MSG, strlen(WELCOME_MSG), 0) != strlen(WELCOME_MSG)) {
                perror("send");
            }

            // add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                // if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    if (!(sessions[i] = opendir(DATA_FOLDER))) {
                        fprintf(stderr, "Cannot open directory '%s'\n", DATA_FOLDER);
                        exit(EXIT_FAILURE);
                    }
                    if (workdirs[i] == NULL)
                        workdirs[i] = (char *) malloc((strlen(DATA_FOLDER) + 1) * sizeof(char));
                    else
                        workdirs[i] = (char *) realloc(workdirs[i], (strlen(DATA_FOLDER) + 1) * sizeof(char));
                    strcpy(workdirs[i], DATA_FOLDER);
                    printf("DEBUG: Adding to list of sockets as %d\n", i);
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
                    printf("DEBUG: Request: %s\n", buffer);
                    if (req_ret == 0) {
                        if (args_request.we_wordc == 0)
                            continue;
                        switch (hash(args_request.we_wordv[0])) {
                            case HASH_LS:
                                while ((dir = readdir(sessions[i])) != NULL)
                                    send(sd, strcat(dir->d_name, "\n"), strlen(dir->d_name) + 1, 0);
                                rewinddir(sessions[i]);
                                break;
                            case HASH_GET:
                                if (args_request.we_wordc < 2) {
                                    send(sd, "Specify a filename\n", strlen("Specify a filename\n"), 0);
                                } else {
                                    filename = args_request.we_wordv[1];
                                     if (filepath)
                                        filepath = (char *) realloc(filepath, (strlen(workdirs[i]) + strlen(filename) + 1) * sizeof(char));
                                    else
                                        filepath = (char *) malloc((strlen(workdirs[i]) + strlen(filename) + 1) * sizeof(char));
                                    strcpy(filepath, workdirs[i]);
                                    strcat(filepath, filename);
                                    FILE *fp = fopen(filepath, "rb");
                                    if (fp == NULL) {
                                        send(sd, "File not found\n", strlen("File not found\n"), 0);
                                    } else {
                                        fseek(fp, 0L, SEEK_END);
                                        int sz = ftell(fp);
                                        rewind(fp);
                                        char *filedata = malloc(sz);
                                        fread(filedata, sz, 1, fp);
                                        fclose(fp);
                                        send(sd, filedata, sz, 0);
                                    }
                                }
                                break;
                            case HASH_CD:
                                if (args_request.we_wordc < 2) {
                                    send(sd, "Specify a directory\n", strlen("Specify a directory\n"), 0);
                                } else {
                                    filename = args_request.we_wordv[1];
                                    if (strcmp(workdirs[i], DATA_FOLDER) == 0 && strcmp(filename, "..") == 0) {
                                        send(sd, "Cannot go back\n", strlen("Cannot go back\n"), 0);
                                    } else {
                                        size_t new_length = strlen(workdirs[i]) + strlen(filename) + 2;
                                        workdirs[i] = (char *) realloc(workdirs[i], new_length * sizeof(char));
                                        strcat(workdirs[i], "/");
                                        strcat(workdirs[i], filename);
                                        if (sessions[i] = opendir(workdirs[i])) {
                                            send(sd, "Directory changed\n", strlen("Directory changed\n"), 0);
                                        } else {
                                            send(sd, "Directory not found\n", strlen("Directory not found\n"), 0);
                                            new_length = strlen(workdirs[i]) - strlen(filename) + 1;
                                            workdirs[i] = (char *) realloc(workdirs[i], new_length * sizeof(char));
                                            workdirs[new_length] = '\0';
                                        }
                                    }
                                }
                                break;
                            default:
                                send(sd, "Unknown command", strlen("Unknown command"), 0);
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
    close(master_socket);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i])
            closedir(sessions[i]);
        free(workdirs[i]);
    }
    return 0;
}
