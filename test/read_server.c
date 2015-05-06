#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <string.h>

int main(int argc, char **argv)
{
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    int fd;
    struct sockaddr c_address;
    int c_addr_len;
    char buf[1024];
    int port;


    if (argc != 2 || sscanf(argv[1], "%d", &port) <= 0) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return -4;
    }

    if (sock < 0) {
        fprintf(stderr, "cannot create socket\n");
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, \
             sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "cannot bind port\n");
        return -2;
    }
    if (listen(sock, 5) < 0) {
        fprintf(stderr, "cannot listen on port\n");
        return -3;
    }
    fd = accept(sock, &c_address, &c_addr_len);
    while(1) {
        memset(buf, 0, 1024);
        read(fd, &buf, 1024);
        printf("%s\n", buf);
    }

}
