#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <pthread.h>
#include "uthash.h"

#if !defined(NDEBUG)
#define debug_log(...) do { fprintf(stdout, __VA_ARGS__); } while (0)
#else
#define debug_log(...) do {} while (0)
#endif

struct client {
    int fake_fd;        //key
    int fd;
    int client_id;
    int is_droped;

    /*
    struct sockaddr sock_addr;
    socklen_t sock_len;
    */

    time_t time;


    UT_hash_handle hh;
};

struct id_fd {
    int id;             //key
    int fd;
    UT_hash_handle hh;
};

typedef struct client client_t;
typedef struct id_fd id_fd_t;

client_t *cli_fd_table = NULL;
id_fd_t *id_fd_table = NULL;

int heart_on = 0;


void mw_init_recv_heart(void *p)
{
    struct sockaddr_in address;
    struct sockaddr_in peer_addr;
    socklen_t peer_len;
    int port = 8992;
    int sock;

    char buf[20];
    int client_id;
    client_t *current, *tmp_item;
    id_fd_t *id_key;
    time_t last_time;
    time_t cur_time;
    double passed_time;

    //use for timeout
    struct timeval timeout;
    timeout.tv_sec = 6;
    timeout.tv_usec = 0;


    /* create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
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

    //设置超时
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        fprintf(stderr, "setsockopt failed\n");

    debug_log("heart start\n");

    time(&last_time);
    while (1) {
        //接收心跳包 ID + 'H'
        memset(buf, 0, 20);
        recvfrom(sock, buf, 20, 0,
                (struct sockaddr *)&peer_addr, &peer_len);
        if (buf[sizeof(int)] == 'H') {
            buf[sizeof(int)] = 0;
            memcpy(&client_id, buf, sizeof(int));
            HASH_FIND_INT(id_fd_table, &client_id, id_key);
            if (id_key) {
                HASH_FIND_INT(cli_fd_table, &(id_key->fd), current);
                assert(current != NULL);
            }
            if (id_key != NULL && current != NULL) {
                time(&(current->time));
            }
        }

        time(&cur_time);
        passed_time = difftime(cur_time, last_time);
        debug_log("debug::heart: passed %f\n", passed_time);
        if (passed_time >= 6) {
            debug_log("start iter\n");
            //遍历hash表，检查每个客户端的连接是否存活
            HASH_ITER(hh, cli_fd_table, current, tmp_item) {
                if (current->time != 0 && \
                        difftime(cur_time, current->time) > 8) {
                    debug_log("id %d is droped\n", current->client_id);
                    current->is_droped = 1;
                    shutdown(current->fd, 2);
                }
            }
            last_time = cur_time;
        }
    }


}



int mw_socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    return fd;
}

int mw_bind(int fd, struct sockaddr *addr, socklen_t addr_len)
{
    return bind(fd, addr, addr_len);
}

int mw_listen(int fd, int n) {
    return listen(fd, n);
}

int mw_accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
    int c_fd = accept(fd, addr, addr_len);
    int client_id;
    client_t *client;
    id_fd_t *id_key;
    int fake_fd;

    pthread_t thread;
    void *p = NULL;

    if (!heart_on) {
        pthread_create(&thread, 0, (void *)mw_init_recv_heart, p);
        pthread_detach(thread);
        heart_on = 1;
    }


    srand(time(NULL));

    recv(c_fd, &client_id, sizeof(int), MSG_NOSIGNAL);

    HASH_FIND_INT(id_fd_table, &client_id, id_key);
    if (id_key) {
        HASH_FIND_INT(cli_fd_table, &id_key->fd, client);
        assert(client != NULL);
    }
    if (id_key == NULL || client == NULL) {
        client = (client_t *)malloc(sizeof(client_t));

        fake_fd = rand() % 10000000 + 10000000;
        client->fake_fd = fake_fd;
        client->fd = c_fd;
        client->is_droped = 0;
        client->client_id = client_id;

        client->time = 0;
        time(&client->time);

        id_key = (id_fd_t *)malloc(sizeof(id_fd_t));
        id_key->id = client_id;
        id_key->fd = client->fake_fd;

        HASH_ADD_INT(cli_fd_table, fake_fd, client);
        HASH_ADD_INT(id_fd_table, id, id_key);
    } else {
        shutdown(client->fd, 2);
        client->fd = c_fd;
        client->is_droped = 0;
    }

    return client->fake_fd;
}

ssize_t mw_recv(int fake_fd, void *buf, size_t n, int flags)
{
    client_t *current;
    int r;

    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    debug_log("debug::recv from id: %d\n", current->client_id);

    r = recv(current->fd, buf, n, flags);

    while (r <= 0) {
        debug_log("debug::%d droped in recv by accept\n", current->client_id);
        current->is_droped = 1;
        while (current->is_droped) {
            sleep(1);
        }
        r = recv(current->fd, buf, n, flags);
    }

    return r;
}

ssize_t mw_send(int fake_fd, const void *buf, size_t n, int flags)
{
    client_t *current;
    int r;

    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    r = send(current->fd, buf, n, flags);
    while (r <= 0) {
        debug_log("debug::%d droped in send\n", current->client_id);
        current->is_droped = 1;
        while (current->is_droped) {
            sleep(1);
        }
        r = send(current->fd, buf, n, flags);
    }

    return r;
}

