#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>

#include "utils.h"
#include "djb2.h"

#define DEST_FOLDER "recv"
#define DEST_FOLDER_LEN 4

int main() {
    char buffer[BSIZE + 1];
    char* dest = NULL;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        serr("ERROR opening socket\n");
    struct hostent* server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(PORT);
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)))
        serr("ERROR connection failed\n");
    
    printf("Connected\n");

    recv_wrapper(sockfd, buffer);
    printf("%s\n", buffer);

    while (1) {
        fgets(buffer, BSIZE, stdin);
        send(sockfd, buffer, strlen(buffer), 0);
        if (strcmp(buffer, "exit\n") == 0) {
            recv_wrapper(sockfd, buffer);
            printf("%s\n", buffer);
            printf("Closing connection\n");
            close(sockfd);
            return 0;
        } else if (strncmp(buffer, "get", 3) == 0) {
            bool result = false;
            int n = read(sockfd, &result, sizeof(bool));
            if (n < 0)
                serr("ERROR reading result\n");
            recv_wrapper(sockfd, buffer);
            if (!result) {
                printf("%s\n", buffer);
                continue;
            }
            size_t length = 0;
            n = read(sockfd, &length, sizeof(size_t));
            if (n < 0)
                serr("ERROR reading length\n");
            printf("Receiving %s of size %zu\n", buffer, length);
            if (dest)
                dest = realloc(dest, DEST_FOLDER_LEN + strlen(buffer) + 2);
            else
                dest = malloc(DEST_FOLDER_LEN + strlen(buffer) + 2);
            snprintf(dest, DEST_FOLDER_LEN + strlen(buffer) + 2, "%s/%s", DEST_FOLDER, buffer);
            FILE* fp = fopen(dest, "wb");
            if (fp == NULL) {
                fprintf(stderr, "Could not open file %s\n", dest);
                exit(1);
            }
            printf("Writing to %s\n", dest);
            size_t arr_size = 0;
            while (arr_size < length) {
                int n = read(sockfd, buffer, BSIZE);
                if (n < 0)
                    serr("ERROR reading buffer\n");
                buffer[n] = 0;
                fwrite(buffer, 1, n, fp);
                arr_size += n;
            }
            printf("Received %zu bytes\n", arr_size);
            fclose(fp);
        } else {
            recv_wrapper(sockfd, buffer);
            printf("%s\n", buffer);
        }
        buffer[0] = 0;
    }
}