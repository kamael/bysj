#include <stdio.h>
#include <stdlib.h>
//#include <sys/socket.h>
//#include <linux/in.h>
#include <string.h>

#include "mw/server.h"

void *process(void *p_fd)
{
    char buffer[1024];
    int r, len;
    while (1) {
        printf(">");
        memset(buffer, 0, 1024);
        scanf("%20s", buffer);
        len = strlen(buffer);
        r = mw_send(*(int *)p_fd, buffer, len, 0);
        if (r == -1) {
            printf("send data error\n");
            break;
        }

    }

    return 0;
}


int main(int argc, char **argv)
{
    int sock = mw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    int *fd;
    struct sockaddr c_address;
    unsigned int c_addr_len;
    int port;

    pthread_t thread;


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
    if (mw_bind(sock, (struct sockaddr *)&address, \
             sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "cannot bind port\n");
        return -2;
    }
    if (mw_listen(sock, 5) < 0) {
        fprintf(stderr, "cannot listen on port\n");
        return -3;
    }
    while(1) {
        fd = (int *)malloc(sizeof(int));
        *fd = mw_accept(sock, &c_address, &c_addr_len);
        pthread_create(&thread, 0, process, (void *)fd);
        pthread_detach(thread);
    }

}
