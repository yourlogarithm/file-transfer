#define BSIZE 4096
#define PORT 8080

void serr(const char *msg);

void send_wrapper(int sd, char* msg, size_t length);

int recv_wrapper(int sd, char* buffer);
