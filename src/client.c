#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "utils.c"


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage `%s` <hostname> <port>\n", argv[0]);
        exit(1);
    }
    int portno = atoi(argv[2]);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        err("ERROR opening socket\n");
    struct hostent* server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)))
        err("ERROR connection failed\n");
    
    printf("Connected to %s:%d\n", argv[1], portno);

    int n;
    size_t length;
    n = read(sockfd, &length, sizeof(length));
    if (n < 0)
        err("ERROR reading length\n");
    printf("Length: %zu\n", length);

    char* buffer = malloc(length);
    n = read(sockfd, buffer, length);
    if (n < 0)
        err("ERROR reading buffer\n");

    printf("Buffer: %s\n", buffer);

    printf("Closing connection\n");
    close(sockfd);
    return 0;
}