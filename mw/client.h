#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <pthread.h>
#include "uthash.h"

struct client {
    int fd;             // key
    int client_id;
    int is_droped;
    UT_hash_handle hh;
};

typedef struct client client_t;

client_t *cli_table = NULL;



void *mw_init_heart(void *cur)
{
    struct sockaddr_in address;

    struct sockaddr cnn_address;
    socklen_t cnn_addr_len;
    int port = 8992;
    int sock, fd;
    char buffer[20];

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0) {
        fprintf(stderr, "error: cannot create socket\n");
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "error: cannot bind socket\n");
    }

    /* listen on port */
    if (listen(sock, 5) < 0) {
        fprintf(stderr, "error: cannot listen on port\n");
    }

    printf("ready and listening\n");

    fd = accept(sock, &cnn_address, &cnn_addr_len);
    if (fd <= 0) {
    }

    //read(fd, buf, 20, MSG_NOSIGNAL);

}




int mw_socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    int is_init = 0;
    client_t *current;

    srand(time(NULL));
    current = (client_t *)malloc(sizeof(client_t));
    current->fd = fd;
    current->client_id = rand();
    current->is_droped = 0;

    HASH_ADD_INT(cli_table, fd, current);

    return fd;
}

int mw_connect(int fd, struct sockaddr *addr, socklen_t len)
{
    pthread_t thread;

    client_t *current;
    HASH_FIND_INT(cli_table, &fd, current);

    int r = connect(fd, addr, len);

    pthread_create(&thread, 0, mw_init_heart, (void *)current);
    pthread_detach(thread);

    send(   fd,
            &(current->client_id),
            sizeof(current->client_id),
            MSG_NOSIGNAL);

    current->is_droped = 0;

    return r;
}

ssize_t mw_send(int fd, const void *buf, size_t n, int flags)
{
    int r = send(fd, buf, n, flags);
    if (r == -1) {
        //TODO: reconnect

        return -1;
    }

    return r;
}

ssize_t mw_recv(int fd, void *buf, size_t n, int flags)
{
    return recv(fd, buf, n, flags);
}

int mw_close(int fd)
{
    client_t *current;
    HASH_FIND_INT(cli_table, &fd, current);

    free(current);
    HASH_DEL(cli_table, current);

    return close(fd);
}


