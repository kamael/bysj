#include <stdio.h>
#include <stdlib.h>
//#include <sys/socket.h>
//#include <linux/in.h>
#include <string.h>

#include "../mw/server.h"

void *process(void *p_fd)
{
    char buffer[1024];
    char input[1024];
    int len;
    int r;
    while (1) {
        memset(buffer, 0, 1024);
        printf("debug::fd:%d\n", *(int *)p_fd);

        r = mw_recv(*(int *)p_fd, buffer, 1024, MSG_NOSIGNAL);
        if (r <= 0)
            break;
        printf("%s\n", buffer);
        memset(input, 0, 1024);
        scanf("%100s", input);
        len = strlen(input);
        mw_send(*(int *)p_fd, input, len, MSG_NOSIGNAL);
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
