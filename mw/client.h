#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <pthread.h>

struct client {
    int is_droped;
    int client_id;
    int fd;
    struct client* next;
};

typedef struct client client_t;
client_t *head;
head = NULL;

int mw_socket(int domain, int type, int protocol)
{
    client_t *cur = head;
    int fd = socket(domain, type, protocol);
    while (cur)
        cur = cur->next;

    srand(time(NULL));
    cur = (client_t *)malloc(sizeof(client_t));
    cur->is_droped = 0;
    cur->client_id = rand();
    cur->fd = fd;
    cur->next = NULL;

    return fd;
}

int mw_connect(int fd, struct sockaddr *addr, socklen_t len)
{
    pthread_t thread;
    client_t *cur = head;

    connect(fd, addr, len);

    while (cur && cur->fd != fd)
        cur = cur->next;

    assert(cur != NULL);

    pthread_create(&thread, 0, mw_init_heart, cur);
    pthread_detach(thread);

    send(fd, &(cur->client_id), sizeof(cur->client_id), MSG_NOSIGNAL);

    cur->is_droped = 0;
}

ssize_t mw_send(int fd, const void *buf, size_t n, int flags)
{
    if (send(fd, buf, n, flags) == -1) {
        printf("send data error\n");
        //TODO: reconnect
    }
}

ssize_t mw_recv(int fd, void *buf, size_t n, int flags)
{
    recv(fd, buf, n, flags);
}

int close(int fd)
{
    client_t *cur = head;
    client_t *prev = NULL;

    assert(head);
    while (cur && cur->fd != fd) {
        prev = cur;
        cur = cur->next;
    }

    if (!prev) {
        free(head);
        head = NULL;
    } else {
        prev->next = cur->next;
        free(cur);
    }

    close(fd);
}


int mw_init_heart(client_t *cur)
{
    struct sockaddr_in address;
    socklen_t addr_len;
    int port = 8992;
    int sock, fd;
    char buffer[20];

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0) {
        fprintf(stderr, "error: cannot create socket\n");
        return -3;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "error: cannot bind socket\n");
        return -4;
    }

    /* listen on port */
    if (listen(sock, 5) < 0) {
        fprintf(stderr, "error: cannot listen on port\n");
        return -5;
    }

    printf("ready and listening\n");

    fd = accept(sock, &address, &addr_len);
    if (fd <= 0) {
        return -6;
    }

    read(fd, buf, 20, MSG_NOSIGNAL);

}


