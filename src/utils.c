#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include "utils.h"


void serr(const char* msg) {
    perror(msg);
    exit(1);
}

void send_wrapper(int sd, char* msg, size_t length) {
    if (!length)
        length = strlen(msg);
    send(sd, &length, sizeof(size_t), 0);
    send(sd, msg, strlen(msg), 0);
}

int recv_wrapper(int sd, char* buffer) {
    size_t length = 0;
    int n = read(sd, &length, sizeof(size_t));
    if (n < 0)
        serr("ERROR reading length\n");
    n = read(sd, buffer, length);
    if (n < 0)
        serr("ERROR reading buffer\n");
    buffer[length] = 0;
    return n;
}