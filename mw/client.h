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
    int fake_fd;        //key
    int fd;
    int client_id;
    int is_droped;

    struct sockaddr *sock_addr;
    socklen_t *sock_len;

    int domain, type, protocol;

    UT_hash_handle hh;
};

typedef struct client client_t;

client_t *cli_table = NULL;



void temp_mw_init_heart(void *current)
{
    struct sockaddr_in address;
    struct sockaddr cnn_address;
    socklen_t cnn_addr_len;
    int port = 8992;
    int sock, fd;

    char buf[20];

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

    fd = accept(sock, &cnn_address, &cnn_addr_len);
    if (fd <= 0) {
        fprintf(stderr, "error: cannot acceptt\n");
    }

    while (1) {
        sleep(8);
        recv(fd, buf, 20, MSG_NOSIGNAL);

        //if no msg

    }


}




int mw_socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    client_t *current;

    srand(time(NULL));
    current = (client_t *)malloc(sizeof(client_t));
    current->fake_fd = rand() & 0x1000000;
    current->fd = fd;
    current->client_id = rand();
    current->is_droped = 0;

    current->domain = domain;
    current->type = type;
    current->protocol = protocol;

    HASH_ADD_INT(cli_table, fake_fd, current);

    //DEBUG
    printf("debug::id: %d\n", current->client_id);

    return current->fake_fd;
}

int mw_connect(int fake_fd, struct sockaddr *addr, socklen_t len)
{
    //pthread_t thread;
    int r = -1;

    client_t *current;
    HASH_FIND_INT(cli_table, &fake_fd, current);

    if (current->is_droped) {
        current->fd = socket(current->domain, current->type, current->protocol);

        while(r == -1) {
            sleep(2);
            r = connect(current->fd, addr, len);
        }
        current->is_droped = 0;

    } else {

        current->sock_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
        current->sock_len = (socklen_t *)malloc(sizeof(socklen_t));
        memcpy(current->sock_addr, addr, sizeof(struct sockaddr));
        memcpy(current->sock_len, &len, sizeof(socklen_t));

        r = connect(current->fd, addr, len);

        //pthread_create(&thread, 0, (void *)mw_init_heart, (void *)current);
        //pthread_detach(thread);
    }

    send(   current->fd,
            &(current->client_id),
            sizeof(current->client_id),
            MSG_NOSIGNAL);

    return r;
}


ssize_t mw_send(int fake_fd, const void *buf, size_t n, int flags)
{
    client_t *current;
    HASH_FIND_INT(cli_table, &fake_fd, current);

    int r = send(current->fd, buf, n, flags);
    while (r == -1) {
        current->is_droped = 1;
        mw_connect(fake_fd, current->sock_addr, *current->sock_len);
        r = send(current->fd, buf, n, flags);
    }

    return r;
}

ssize_t mw_recv(int fake_fd, void *buf, size_t n, int flags)
{
    client_t *current;
    HASH_FIND_INT(cli_table, &fake_fd, current);

    return recv(current->fd, buf, n, flags);
}

int mw_close(int fake_fd)
{
    client_t *current;
    HASH_FIND_INT(cli_table, &fake_fd, current);

    free(current->sock_addr);
    free(current->sock_len);
    free(current);
    HASH_DEL(cli_table, current);

    return close(current->fd);
}


